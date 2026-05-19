from unittest.mock import MagicMock, patch
from app.llm_client import LLMClient


def test_complete_calls_openai_with_configured_model():
    fake_response = MagicMock()
    fake_response.choices = [MagicMock(message=MagicMock(content="hello world"))]

    with patch("app.llm_client.OpenAI") as mock_openai:
        mock_client = MagicMock()
        mock_client.chat.completions.create.return_value = fake_response
        mock_openai.return_value = mock_client

        llm = LLMClient(api_key="k", base_url="u", model="solar-pro-3")
        out = llm.complete("say hi")

        assert out == "hello world"
        mock_client.chat.completions.create.assert_called_once()
        kwargs = mock_client.chat.completions.create.call_args.kwargs
        assert kwargs["model"] == "solar-pro-3"
        assert kwargs["messages"][0]["content"] == "say hi"
