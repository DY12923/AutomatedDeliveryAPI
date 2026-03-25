document.addEventListener('DOMContentLoaded', () => {
    // Initial load
    loadBoxes();
    
    // Set up auto-refresh
    setInterval(loadBoxes, 5000);

    // Event listener for dispatch form
    const dispatchButton = document.getElementById('dispatch-btn');
    if (dispatchButton) {
        dispatchButton.addEventListener('click', dispatchOrder);
    }
});

/**
 * Fetches current box statuses from the server and updates the UI.
 */
async function loadBoxes() {
    const container = document.getElementById('boxes-container');
    
    try {
        const response = await fetch('/boxes');
        if (!response.ok) throw new Error('Failed to fetch boxes');
        
        const data = await response.json();
        
        if (!data.boxes || data.boxes.length === 0) {
            container.innerHTML = '<div class="loading">No boxes found.</div>';
            return;
        }

        renderBoxes(data.boxes);
    } catch (error) {
        console.error('Error loading boxes:', error);
        container.innerHTML = '<div class="loading error">Error loading boxes. Please check connection.</div>';
    }
}

/**
 * Renders the box cards in the grid container.
 * @param {Array} boxes List of box objects
 */
function renderBoxes(boxes) {
    const container = document.getElementById('boxes-container');
    container.innerHTML = ''; // Clear current content
    
    const grid = document.createElement('div');
    grid.className = 'boxes-grid';
    
    boxes.forEach(box => {
        const card = document.createElement('div');
        card.className = `box-card ${box.status}`;
        
        const studentInfo = box.student_id ? `<span class="student-id">ID: ${box.student_id}</span>` : '';
        
        card.innerHTML = `
            <span class="box-number">Box ${box.box_number}</span>
            <span class="box-status">${box.status}</span>
            ${studentInfo}
        `;
        
        grid.appendChild(card);
    });
    
    container.appendChild(grid);
}

/**
 * Handles the dispatch form submission.
 */
async function dispatchOrder() {
    const studentInput = document.getElementById('student-input');
    const messageEl = document.getElementById('dispatch-message');
    const student_id = studentInput.value.trim();

    if (!student_id) {
        showMessage('Please enter a student ID!', 'error');
        return;
    }

    try {
        // Disable button during request
        const btn = document.getElementById('dispatch-btn');
        btn.disabled = true;
        btn.textContent = 'Dispatching...';

        const response = await fetch('/dispatch', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ student_id: student_id })
        });

        const data = await response.json();

        if (!response.ok) {
            throw new Error(data.error || 'Dispatch failed');
        }

        showMessage(`Successfully dispatched to Box ${data.box_number}!`, 'success');
        studentInput.value = '';
        loadBoxes();
    } catch (error) {
        showMessage(`Error: ${error.message}`, 'error');
    } finally {
        const btn = document.getElementById('dispatch-btn');
        btn.disabled = false;
        btn.textContent = 'Dispatch';
    }
}

/**
 * Helper to show temporary status messages.
 * @param {string} text Message text
 * @param {string} type 'success' or 'error'
 */
function showMessage(text, type) {
    const messageEl = document.getElementById('dispatch-message');
    messageEl.textContent = text;
    messageEl.className = type;
    messageEl.style.display = 'block';

    // Hide message after 5 seconds
    setTimeout(() => {
        messageEl.style.display = 'none';
    }, 5000);
}
