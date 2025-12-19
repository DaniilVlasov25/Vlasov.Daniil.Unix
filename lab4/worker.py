from confluent_kafka import Consumer
import redis
import json
import hashlib
import signal
import os

shutdown = False

def signal_handler(signum, frame):
    global shutdown
    print(f"Получен сигнал {signum}. Завершаем приём новых задач...")
    shutdown = True

signal.signal(signal.SIGTERM, signal_handler)
signal.signal(signal.SIGINT, signal_handler)

worker_id = os.environ.get('HOSTNAME', 'hash-worker')

# Kafka consumer
consumer = Consumer({
    'bootstrap.servers': 'kafka:9092',
    'group.id': 'hash-workers',
    'auto.offset.reset': 'earliest',
    'enable.auto.commit': True
})
consumer.subscribe(['hash_tasks'])

redis_client = redis.Redis(host='redis', port=6379, db=0)

print(f"Hash worker {worker_id} started.")

try:
    while not shutdown:
        msg = consumer.poll(timeout=1.0)
        if msg is None or msg.error():
            continue

        try:
            task = json.loads(msg.value().decode('utf-8'))
            task_id = task['task_id']
            text = task['text']

            print(f"[{worker_id}] Hashing task {task_id}: '{text[:50]}...'")

            sha256_hash = hashlib.sha256(text.encode('utf-8')).hexdigest()

            redis_client.setex(
                f'result:{task_id}',
                600,
                json.dumps({'sha256': sha256_hash, 'worker': worker_id})
            )

            print(f"[{worker_id}] Task {task_id} done. Hash: {sha256_hash[:16]}...")

        except Exception as e:
            print(f"[{worker_id}] Error processing task: {e}")

except Exception as e:
    print(f"Critical error: {e}")
finally:
    print("Closing Kafka consumer...")
    consumer.close()
    print("Worker stopped.")
