import queue
from datetime import datetime
from app.agent import AgentTask, AgentState, AgentKind


INPUT_PLACEHOLDER = "{INPUT}"


class Executor:
    """Executes ONE agent end-to-end:
        (pipeline input?) → quota gate → LLM → optional sandbox → (pipeline output?)

    Designed to be called by worker threads. All collaborators are passed in
    so the unit is easy to test with mocks.
    """

    def __init__(self, llm, sandbox, quota, logger, events, bus,
                 pipeline_timeout: float = 30.0):
        self._llm = llm
        self._sandbox = sandbox
        self._quota = quota
        self._log = logger
        self._events = events
        self._bus = bus
        self._pipeline_timeout = pipeline_timeout

    def execute(self, task: AgentTask) -> AgentTask:
        task.state = AgentState.RUNNING
        task.start_time = datetime.now()
        self._log.log(f"Agent {task.agent_id} ({task.name}) started")
        self._publish(task)

        # 1. Pipeline input: if prompt contains {INPUT}, block until upstream sends
        prompt = task.prompt
        if INPUT_PLACEHOLDER in prompt:
            try:
                upstream = self._bus.receive(
                    f"pipe-{task.agent_id}", timeout=self._pipeline_timeout
                )
            except queue.Empty:
                return self._fail(task, AgentState.ERROR,
                                  f"Pipeline input not received within "
                                  f"{self._pipeline_timeout}s")
            prompt = prompt.replace(INPUT_PLACEHOLDER, str(upstream))

        # 2. Quota gate (mutex)
        if not self._quota.try_acquire(1):
            return self._fail(task, AgentState.ERROR, "API quota exhausted")

        # 3. LLM call
        try:
            llm_output = self._llm.complete(prompt)
        except Exception as e:
            return self._fail(task, AgentState.ERROR, f"LLM error: {e}")

        # 4. Optional sandbox for CODE agents
        if task.kind == AgentKind.LLM:
            final_result = llm_output
        else:
            sb = self._sandbox.run(llm_output)
            if sb.timed_out:
                return self._fail(task, AgentState.TIMEOUT,
                                  "Sandbox execution timed out")
            if sb.exit_code != 0:
                return self._fail(task, AgentState.ERROR,
                                  f"Sandbox exit {sb.exit_code}: {sb.stderr.strip()}")
            final_result = f"code:\n{llm_output}\n---\nstdout:\n{sb.stdout}"

        task.result = final_result

        # 5. Pipeline output: publish to downstream if pipe_to is set
        if task.pipe_to is not None:
            try:
                self._bus.send(f"pipe-{task.pipe_to}", final_result, timeout=5)
            except queue.Full:
                self._log.log(
                    f"Agent {task.agent_id} pipeline output dropped: "
                    f"pipe-{task.pipe_to} full"
                )

        return self._done(task)

    def _done(self, task: AgentTask) -> AgentTask:
        task.state = AgentState.DONE
        task.end_time = datetime.now()
        self._log.log(f"Agent {task.agent_id} ({task.name}) completed")
        self._publish(task)
        return task

    def _fail(self, task: AgentTask, state: AgentState, message: str) -> AgentTask:
        task.state = state
        task.error_message = message
        task.end_time = datetime.now()
        self._log.log(f"Agent {task.agent_id} ({task.name}) {state.value}: {message}")
        self._publish(task)
        return task

    def _publish(self, task: AgentTask) -> None:
        self._events.publish(task.to_dict())
