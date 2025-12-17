FROM python:3.9-slim

WORKDIR /app

RUN apt-get update && apt-get install -y librdkafka-dev gcc && rm -rf /var/lib/apt/lists/*

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY api.py worker.py .

CMD ["python", "api.py"]
