from flask import Flask, request, jsonify
from confluent_kafka import Producer
import redis
import uuid
import json

app = Flask(__name__)

producer = Producer({'bootstrap.servers': 'kafka:9092'})

redis_client = redis.Redis(host='redis', port=6379, db=0)

@app.route('/hash', methods=['POST'])
def hash_text():
    try:
        data = request.get_json()
        text = data.get('text')
        if text is None:
            return jsonify({'error': 'Field "text" is required'}), 400

        task_id = str(uuid.uuid4())
        task = {'task_id': task_id, 'text': text}

        producer.produce(
            'hash_tasks',
            key=task_id,
            value=json.dumps(task).encode('utf-8')
        )
        producer.poll(0)

        return jsonify({'task_id': task_id, 'status': 'queued'}), 202

    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/result/<task_id>', methods=['GET'])
def get_result(task_id):
    res = redis_client.get(f'result:{task_id}')
    if res:
        return jsonify({
            'task_id': task_id,
            'status': 'completed',
            'result': json.loads(res)
        })
    return jsonify({'task_id': task_id, 'status': 'processing'}), 202

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
