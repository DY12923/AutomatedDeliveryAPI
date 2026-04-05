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
            c.execute("INSERT INTO boxes (box_number) VALUES (?)", (i,))

    c.execute('''
        CREATE TABLE IF NOT EXISTS deliveries (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            student_id TEXT NOT NULL,
            name TEXT NOT NULL,
            status TEXT DEFAULT 'pending',
            box_number INTEGER NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    ''')

    c.execute('''
        CREATE TABLE IF NOT EXISTS registrations (
            uid TEXT PRIMARY KEY,
            student_id TEXT NOT NULL,
            name TEXT NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    ''')

    conn.commit()
    conn.close()

init_db()

pending_registration = {"student_id": None, "name": None}

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/dispatch', methods=['POST'])
def dispatch():
    data = request.get_json()
    if not data or 'student_id' not in data:
        return jsonify({"error": "Missing student_id"}), 400
    student_id = data['student_id']
    
    conn, c = get_db()

    c.execute("SELECT name FROM registrations WHERE student_id = ?", (student_id,))
    reg = c.fetchone()
    if reg is None:
        conn.close()
        return jsonify({"error": "Student not registered"}), 404
    name, = reg

    c.execute("SELECT box_number FROM boxes WHERE status = 'available' LIMIT 1")
    row = c.fetchone()
    
    if row is None:
        conn.close()
        return jsonify({"error": "All boxes are full"}), 400
    
    box_number, = row
    
    c.execute("UPDATE boxes SET status = 'occupied' WHERE box_number = ?", (box_number,))
    c.execute("INSERT INTO deliveries (student_id, name, box_number) VALUES (?, ?, ?)", (student_id, name, box_number))
    
    conn.commit()
    conn.close()
    
    return jsonify({"message": "Delivery created", "student_id": student_id, "name": name, "box_number": box_number})

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
    if not data or 'student_id' not in data or 'box_number' not in data:
        return jsonify({"error": "Missing student_id or box_number"}), 400
    student_id = data['student_id']
    box_number = data['box_number']

    conn, c = get_db()

    c.execute("UPDATE deliveries SET status = 'claimed' WHERE student_id = ? AND box_number = ? AND status = 'pending'", (student_id, box_number))
    if c.rowcount == 0:
        conn.close()
        return jsonify({"error": "No pending delivery found for this student and box"}), 404
    c.execute("UPDATE boxes SET status = 'available' WHERE box_number = ?", (box_number,))

    conn.commit()
    conn.close()

    return jsonify({"message": "Delivery claimed", "student_id": student_id, "box_number": box_number})

@app.route('/boxes', methods=['GET'])
def boxes():
    conn, c = get_db()
    c.execute('''
        SELECT boxes.box_number, boxes.status, deliveries.student_id, deliveries.name
        FROM boxes
        LEFT JOIN deliveries ON boxes.box_number = deliveries.box_number 
        AND deliveries.status = 'pending'
    ''')
    rows = c.fetchall()
    conn.close()

    result = []
    for row in rows:
        box_number, status, student_id, name = row
        result.append({
            "box_number": box_number,
            "status": status,
            "student_id": student_id,
            "name": name
        })

    return jsonify({"boxes": result})

@app.route('/register/start', methods=['POST'])
def register_start():
    global pending_registration
    data = request.get_json()
    if not data or 'student_id' not in data or 'name' not in data:
        return jsonify({"error": "Missing student_id or name"}), 400
    
    pending_registration = {
        "student_id": data['student_id'],
        "name": data['name']
    }
    return jsonify({"message": "Registration armed", "student_id": data['student_id'], "name": data['name']})

@app.route('/register/pending', methods=['GET'])
def register_pending():
    return jsonify(pending_registration)

@app.route('/register', methods=['POST'])
def register():
    global pending_registration
    data = request.get_json()
    if not data or 'uid' not in data or 'student_id' not in data or 'name' not in data:
        return jsonify({"error": "Invalid request"}), 400
    
    uid = data['uid']
    student_id = data['student_id']
    name = data['name']
    
    conn, c = get_db()
    c.execute("INSERT OR REPLACE INTO registrations (uid, student_id, name) VALUES (?, ?, ?)", (uid, student_id, name))
    conn.commit()
    conn.close()
    
    pending_registration = {"student_id": None, "name": None}
    return jsonify({"message": "Registration successful", "uid": uid, "student_id": student_id, "name": name})

@app.route('/lookup', methods=['GET'])
def lookup():
    uid = request.args.get('uid')
    
    if uid is None:
        return jsonify({"error": "No uid provided"}), 400
    
    conn, c = get_db()
    c.execute("SELECT student_id, name FROM registrations WHERE uid = ?", (uid,))
    row = c.fetchone()
    conn.close()
    
    if row is None:
        return jsonify({"student_id": None, "name": None})
    
    student_id, name = row
    return jsonify({"student_id": student_id, "name": name})

if __name__ == '__main__':
    app.run(debug=False)