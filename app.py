from flask import Flask, request, jsonify, render_template
import sqlite3

app = Flask(__name__)

def get_db():
    conn = sqlite3.connect('database.db')
    c = conn.cursor()
    return conn, c

def init_db():
    conn = sqlite3.connect('database.db')
    c = conn.cursor()
    c.execute('''
        CREATE TABLE IF NOT EXISTS boxes(
        box_number INTEGER PRIMARY KEY,
        status TEXT DEFAULT 'available'
        )
    ''')

    c.execute("SELECT COUNT(*) FROM boxes")
    count, = c.fetchone()
    if count == 0:
        for i in range(1,5):
            c.execute("Insert into boxes (box_number) VALUES (?)", (i,))


    c.execute('''
     CREATE TABLE IF NOT EXISTS deliveries (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            student_id TEXT NOT NULL,
            status TEXT DEFAULT 'pending',
            box_number INTEGER NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    ''')
    conn.commit()
    conn.close()

init_db()

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/dispatch', methods=['POST'])
def dispatch():
    data = request.get_json()
    student_id = data['student_id']
    
    conn, c = get_db()
    
    c.execute("SELECT box_number FROM boxes WHERE status = 'available' LIMIT 1")
    row = c.fetchone()
    
    if row is None:
        conn.close()
        return jsonify({"error": "All boxes are full"}), 400
    
    box_number, = row
    
    c.execute("UPDATE boxes SET status = 'occupied' WHERE box_number = ?", (box_number,))
    c.execute("INSERT INTO deliveries (student_id, box_number) VALUES (?, ?)", (student_id, box_number))
    
    conn.commit()
    conn.close()
    
    return jsonify({"message": "Delivery created", "student_id": student_id, "box_number": box_number})

@app.route('/pending', methods=['GET'])
def pending():
    student_id = request.args.get('student_id')
    
    if student_id is None:
        return jsonify({"error": "No student_id provided"}), 400
    
    conn, c = get_db()
    c.execute("SELECT student_id, box_number FROM deliveries WHERE status = 'pending' AND student_id = ?", (student_id,))
    row = c.fetchone()
    conn.close()
    
    if row is None:
        return jsonify({"student_id": None, "box_number": None})
    
    student_id, box_number = row
    return jsonify({"student_id": student_id, "box_number": box_number})    

@app.route('/claim', methods=['POST'])
def claim():
    data = request.get_json()
    student_id = data['student_id']
    box_number = data['box_number']
    
    conn, c = get_db()
    
    c.execute("UPDATE deliveries SET status = 'claimed' WHERE student_id = ? AND box_number = ? AND status = 'pending'", (student_id, box_number))
    c.execute("UPDATE boxes SET status = 'available' WHERE box_number = ?", (box_number,))
    
    conn.commit()
    conn.close()
    
    return jsonify({"message": "Delivery claimed", "student_id": student_id, "box_number": box_number})

@app.route('/boxes', methods=['GET'])
def boxes():
    conn, c = get_db()
    c.execute('''
                  SELECT boxes.box_number, boxes.status, deliveries.student_id 
                  FROM boxes
                  LEFT JOIN deliveries ON boxes.box_number = deliveries.box_number 
                  AND deliveries.status = 'pending'
                  ''')
    rows = c.fetchall()
    conn.close()

    result = []
    for row in rows:
        box_number, status, student_id = row
        result.append({
            "box_number":box_number,
            "status": status,
            "student_id": student_id
            })

    return jsonify({"boxes": result})

if __name__ == '__main__':
    app.run(debug=True)