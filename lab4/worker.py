from confluent_kafka import Consumer
import redis
import json
import hashlib
import time
import os

consumer = Consumer({
    'bootstrap.servers': 'kafka:9092',
    'group.id': 'hash-workers',
    'auto.offset.reset': 'earliest',
    'enable.auto.commit': True
})
consumer.subscribe(['hash_tasks'])

redis_client = redis.Redis(host='redis', port=6379, db=0)
worker_id = os.environ.get('HOSTNAME', 'hash-worker')

print(f"Hash worker {worker_id} started.")

try:
    while True:
        msg = consumer.poll(timeout=1.0)
        if msg is None or msg.error():
            continue

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

except KeyboardInterrupt:
    pass
finally:
    consumer.close()
