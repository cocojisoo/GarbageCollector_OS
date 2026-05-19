// ---------- agent table + SSE ----------

const agentMap = new Map();

function renderAgents() {
  const tbody = document.querySelector("#agents tbody");
  const rows = [...agentMap.values()].sort((a, b) => a.agent_id - b.agent_id);
  tbody.innerHTML = rows.map(a => `
    <tr>
      <td>${a.agent_id}</td>
      <td>${a.name}</td>
      <td>${a.kind}</td>
      <td class="state-${a.state}">${a.state}</td>
      <td>${a.priority}</td>
      <td>${a.pipe_to ?? ""}</td>
      <td><pre>${escapeHtml((a.result || a.error_message || "")).slice(0, 600)}</pre></td>
    </tr>
  `).join("");
}

function escapeHtml(s) {
  return s.replace(/[&<>"']/g, c => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", "\"": "&quot;", "'": "&#39;"
  }[c]));
}

async function refreshLogs() {
  const r = await fetch("/logs");
  const data = await r.json();
  document.querySelector("#logs").textContent = data.logs.join("\n");
}

async function refreshAll() {
  const r = await fetch("/agents");
  const data = await r.json();
  agentMap.clear();
  for (const a of data.agents) agentMap.set(a.agent_id, a);
  renderAgents();
  refreshLogs();
}

const es = new EventSource("/events");
es.onmessage = (ev) => {
  const a = JSON.parse(ev.data);
  agentMap.set(a.agent_id, a);
  renderAgents();
  refreshLogs();
};

// ---------- create agent ----------

document.querySelector("#create-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  const body = Object.fromEntries(new FormData(e.target).entries());
  ["priority", "timeout"].forEach(k => body[k] = Number(body[k]));
  body.pipe_to = body.pipe_to ? Number(body.pipe_to) : null;
  await fetch("/agents", {
    method: "POST", headers: {"Content-Type": "application/json"},
    body: JSON.stringify(body),
  });
  e.target.reset();
});

// ---------- policy switch ----------

const policySel = document.querySelector("#policy");
fetch("/policy").then(r => r.json()).then(d => { policySel.value = d.policy; });
policySel.addEventListener("change", async (e) => {
  const r = await fetch("/policy", {
    method: "POST", headers: {"Content-Type": "application/json"},
    body: JSON.stringify({policy: e.target.value}),
  });
  const d = await r.json();
  const s = document.querySelector("#policy-status");
  s.textContent = `→ ${d.policy}`;
  setTimeout(() => s.textContent = "", 1500);
});

// ---------- clear ----------

document.querySelector("#clear").onclick = async () => {
  await fetch("/agents", {method: "DELETE"});
  agentMap.clear();
  renderAgents();
  refreshLogs();
};

// ---------- simulator ----------

function parseJobs(text) {
  return text.trim().split("\n").map(line => {
    const [id, burst, arrival = "0", priority = "1"] = line.trim().split(/\s+/);
    return {
      id, burst: Number(burst), arrival: Number(arrival), priority: Number(priority),
    };
  });
}

function renderGantt(gantt) {
  const container = document.querySelector("#sim-gantt");
  container.innerHTML = "";
  if (!gantt.length) return;
  const totalEnd = gantt[gantt.length - 1].end;
  for (const slice of gantt) {
    const dur = slice.end - slice.start;
    const bar = document.createElement("div");
    bar.className = "sim-bar";
    bar.style.flexGrow = String(dur);
    bar.innerHTML = `${slice.id}<small>${slice.start}→${slice.end}</small>`;
    container.appendChild(bar);
  }
  const totalLabel = document.createElement("div");
  totalLabel.className = "hint";
  totalLabel.textContent = `total time = ${totalEnd}`;
  container.appendChild(totalLabel);
}

document.querySelector("#sim-run").onclick = async () => {
  const policy = document.querySelector("#sim-policy").value;
  const quantum = Number(document.querySelector("#sim-quantum").value);
  const jobs = parseJobs(document.querySelector("#sim-jobs").value);
  const body = { policy, jobs };
  if (policy === "rr") body.quantum = quantum;
  const r = await fetch("/simulate", {
    method: "POST", headers: {"Content-Type": "application/json"},
    body: JSON.stringify(body),
  });
  if (!r.ok) {
    alert(await r.text());
    return;
  }
  const data = await r.json();
  renderGantt(data.gantt);
};

refreshAll();
