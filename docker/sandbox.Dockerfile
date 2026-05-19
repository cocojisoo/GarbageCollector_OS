FROM python:3.11-slim

# Non-root user for defense-in-depth
RUN useradd -m -u 1000 sandboxuser

WORKDIR /sandbox
USER sandboxuser

# Code is piped in on stdin; nothing baked into the image
# -I = isolated mode (ignore env vars, user site-packages, $PYTHONPATH)
ENTRYPOINT ["python", "-I", "-"]
