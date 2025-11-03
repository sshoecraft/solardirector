#!/usr/bin/env python3
"""
Simple Flask app for Service Controller
- Reads JSON data files (no SC binary invocation)
- Manages agent list through file updates
- Clean separation of concerns
"""

import json
import os
import time
import glob
from flask import Flask, render_template, jsonify, request
from pathlib import Path

app = Flask(__name__)
app.config['DEBUG'] = True

# Configuration
CONFIG = {
    'data_file': '/tmp/sc_data.json',
    'managed_agents_file': '/tmp/sc_managed_agents.json',
    'agent_dir': '/Users/steve/bin',
    'relaxed_validation': True
}

def read_json_file(filepath, default=None):
    """Read JSON file with error handling"""
    try:
        if os.path.exists(filepath):
            with open(filepath, 'r') as f:
                return json.load(f)
    except Exception as e:
        print(f"Error reading {filepath}: {e}")
    return default or {}

def write_json_file(filepath, data):
    """Write JSON file with error handling"""
    try:
        with open(filepath, 'w') as f:
            json.dump(data, f, indent=2)
        return True
    except Exception as e:
        print(f"Error writing {filepath}: {e}")
        return False

def get_available_agents():
    """Scan agent directory for available agents"""
    available_agents = []
    
    if os.path.exists(CONFIG['agent_dir']):
        # Find all executable files in agent directory
        for filepath in glob.glob(os.path.join(CONFIG['agent_dir'], '*')):
            if os.path.isfile(filepath) and os.access(filepath, os.X_OK):
                name = os.path.basename(filepath)
                # Skip common non-agent files
                if name not in ['README', 'Makefile', '.DS_Store'] and not name.startswith('.'):
                    available_agents.append({
                        'name': name,
                        'path': filepath
                    })
    
    return sorted(available_agents, key=lambda x: x['name'])

def validate_agent(agent_path):
    """Validate agent by checking if it responds to -I flag"""
    if not CONFIG['relaxed_validation']:
        return True, []
    
    warnings = []
    
    try:
        import subprocess
        result = subprocess.run([agent_path, '-I'], 
                              capture_output=True, 
                              text=True, 
                              timeout=10)
        
        if result.returncode != 0:
            warnings.append(f"Agent returned non-zero exit code ({result.returncode})")
        
        if result.stdout:
            try:
                json.loads(result.stdout)
            except json.JSONDecodeError as e:
                warnings.append(f"Failed to parse -I output as JSON: {e}")
        
    except subprocess.TimeoutExpired:
        warnings.append("Agent timed out during validation")
    except Exception as e:
        warnings.append(f"Validation error: {e}")
    
    return True, warnings  # Always allow in relaxed mode

# Routes
@app.route('/')
def dashboard():
    return render_template('simple_dashboard.html')

@app.route('/api/agents')
def get_agents():
    """Get current agent status from SC data file"""
    data = read_json_file(CONFIG['data_file'], {'agents': {'agents': []}})
    return jsonify(data.get('agents', {'agents': []}))

@app.route('/api/system/status')
def get_system_status():
    """Get system status from SC data file"""
    data = read_json_file(CONFIG['data_file'], {'system': {}})
    return jsonify(data.get('system', {}))

@app.route('/api/available-agents')
def get_available_agents_api():
    """Get list of available agents for adding"""
    available = get_available_agents()
    managed_data = read_json_file(CONFIG['managed_agents_file'], {'agents': []})
    managed_names = {agent['name'] for agent in managed_data.get('agents', [])}
    
    # Filter out already managed agents
    filtered = [agent for agent in available if agent['name'] not in managed_names]
    
    return jsonify({'agents': filtered})

@app.route('/api/agents/add', methods=['POST'])
def add_agent():
    """Add an agent to the managed list"""
    try:
        data = request.get_json()
        agent_name = data.get('name')
        agent_path = data.get('path')
        
        if not agent_name or not agent_path:
            return jsonify({'error': 'Name and path required'}), 400
        
        # Load current managed agents
        managed_data = read_json_file(CONFIG['managed_agents_file'], {'agents': []})
        agents = managed_data.get('agents', [])
        
        # Check if agent already exists
        if any(agent['name'] == agent_name for agent in agents):
            return jsonify({'error': 'Agent already managed'}), 409
        
        # Validate agent if not in relaxed mode
        valid, warnings = validate_agent(agent_path)
        if not valid:
            return jsonify({'error': 'Agent validation failed', 'warnings': warnings}), 400
        
        # Add new agent
        new_agent = {
            'name': agent_name,
            'path': agent_path,
            'enabled': True,
            'added': time.time()
        }
        
        if warnings:
            new_agent['validation_warnings'] = warnings
        
        agents.append(new_agent)
        managed_data['agents'] = agents
        
        # Save updated list
        if write_json_file(CONFIG['managed_agents_file'], managed_data):
            return jsonify({'message': 'Agent added successfully'})
        else:
            return jsonify({'error': 'Failed to save agent list'}), 500
            
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/api/agents/remove', methods=['POST'])
def remove_agent():
    """Remove an agent from the managed list"""
    try:
        data = request.get_json()
        agent_name = data.get('name')
        
        if not agent_name:
            return jsonify({'error': 'Agent name required'}), 400
        
        # Load current managed agents
        managed_data = read_json_file(CONFIG['managed_agents_file'], {'agents': []})
        agents = managed_data.get('agents', [])
        
        # Find and remove agent
        original_count = len(agents)
        agents = [agent for agent in agents if agent['name'] != agent_name]
        
        if len(agents) == original_count:
            return jsonify({'error': 'Agent not found'}), 404
        
        managed_data['agents'] = agents
        
        # Save updated list
        if write_json_file(CONFIG['managed_agents_file'], managed_data):
            return jsonify({'message': 'Agent removed successfully'})
        else:
            return jsonify({'error': 'Failed to save agent list'}), 500
            
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/api/managed-agents')
def get_managed_agents():
    """Get list of managed agents"""
    managed_data = read_json_file(CONFIG['managed_agents_file'], {'agents': []})
    return jsonify(managed_data)

@app.route('/api/agents/logs/<agent_name>')
def get_agent_logs(agent_name):
    """Get recent log entries for an agent"""
    try:
        import os
        from flask import Response
        log_file = f'/tmp/{agent_name}.log'
        
        if not os.path.exists(log_file):
            if request.args.get('format') == 'html':
                return f"<html><body><h1>Error</h1><p>Log file not found: {log_file}</p></body></html>", 404
            return jsonify({'error': f'Log file not found: {log_file}'}), 404
        
        lines = request.args.get('lines', 100, type=int)
        
        # Read last N lines of the log file
        try:
            import subprocess
            result = subprocess.run(['tail', '-n', str(lines), log_file], 
                                  capture_output=True, text=True, timeout=5)
            if result.returncode == 0:
                log_content = result.stdout
            else:
                # Fallback to Python implementation
                with open(log_file, 'r') as f:
                    all_lines = f.readlines()
                    log_content = ''.join(all_lines[-lines:])
        except Exception:
            # Fallback to Python implementation
            with open(log_file, 'r') as f:
                all_lines = f.readlines()
                log_content = ''.join(all_lines[-lines:])
        
        # Check if HTML format is requested
        if request.args.get('format') == 'html':
            html_content = f"""
<!DOCTYPE html>
<html>
<head>
    <title>Logs for {agent_name} - Real-time</title>
    <style>
        body {{ font-family: monospace; margin: 20px; }}
        .header {{ margin-bottom: 20px; }}
        .log-content {{ 
            background-color: #f5f5f5; 
            padding: 15px; 
            border: 1px solid #ddd; 
            white-space: pre-wrap; 
            max-height: 500px; 
            overflow-y: auto; 
            font-size: 12px;
        }}
        .status {{ 
            margin: 10px 0; 
            padding: 8px; 
            background-color: #e7f3ff; 
            border: 1px solid #b3d7ff; 
            border-radius: 3px;
        }}
        .connected {{ color: green; }}
        .disconnected {{ color: red; }}
    </style>
</head>
<body>
    <div class="header">
        <h1>Logs for {agent_name} - Real-time</h1>
        <p>Log file: {log_file}</p>
        <div class="status">
            Status: <span id="status" class="disconnected">Connecting...</span>
        </div>
    </div>
    <div id="log-content" class="log-content">{log_content}</div>
    <script>
        const logContent = document.getElementById('log-content');
        const statusElement = document.getElementById('status');
        
        // Initialize EventSource for real-time log streaming
        const eventSource = new EventSource('/api/agents/logs/{agent_name}/stream');
        
        eventSource.onopen = function() {{
            statusElement.textContent = 'Connected - Real-time';
            statusElement.className = 'connected';
        }};
        
        eventSource.onmessage = function(event) {{
            // Append new log line
            logContent.textContent += event.data + '\\n';
            // Auto-scroll to bottom
            logContent.scrollTop = logContent.scrollHeight;
        }};
        
        eventSource.onerror = function() {{
            statusElement.textContent = 'Connection lost - Reconnecting...';
            statusElement.className = 'disconnected';
        }};
        
        // Clean up when window is closed
        window.addEventListener('beforeunload', function() {{
            eventSource.close();
        }});
    </script>
</body>
</html>"""
            return Response(html_content, mimetype='text/html')
        
        return jsonify({
            'agent_name': agent_name,
            'log_file': log_file,
            'content': log_content,
            'lines': lines
        })
        
    except Exception as e:
        if request.args.get('format') == 'html':
            return f"<html><body><h1>Error</h1><p>{str(e)}</p></body></html>", 500
        return jsonify({'error': str(e)}), 500

@app.route('/api/agents/logs/<agent_name>/stream')
def stream_agent_logs(agent_name):
    """Stream log updates for an agent using Server-Sent Events"""
    try:
        import os
        log_file = f'/tmp/{agent_name}.log'
        
        if not os.path.exists(log_file):
            return jsonify({'error': f'Log file not found: {log_file}'}), 404
        
        def generate():
            import subprocess
            import time
            
            # Start tailing the log file
            try:
                process = subprocess.Popen(['tail', '-f', log_file], 
                                         stdout=subprocess.PIPE, 
                                         stderr=subprocess.PIPE,
                                         text=True, 
                                         bufsize=1)
                
                while True:
                    line = process.stdout.readline()
                    if line:
                        yield f"data: {line.rstrip()}\n\n"
                    else:
                        time.sleep(0.1)
                        
            except Exception as e:
                yield f"data: ERROR: {str(e)}\n\n"
        
        return app.response_class(generate(), mimetype='text/event-stream', headers={'Cache-Control': 'no-cache', 'Connection': 'keep-alive'})
        
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/api/agents/start', methods=['POST'])
def start_agent():
    """Start an agent process"""
    try:
        data = request.get_json()
        agent_name = data.get('name')
        
        if not agent_name:
            return jsonify({'error': 'Agent name required'}), 400
        
        import subprocess
        
        # Get agent path from managed agents list
        managed_data = read_json_file(CONFIG['managed_agents_file'], {'agents': []})
        agent_path = None
        for agent in managed_data.get('agents', []):
            if agent['name'] == agent_name:
                agent_path = agent['path']
                break
        
        if not agent_path:
            return jsonify({'error': 'Agent path not found'}), 404
        
        # Check if agent is already running
        result = subprocess.run(['pgrep', '-f', agent_name], 
                              capture_output=True, text=True, timeout=5)
        if result.returncode == 0:
            return jsonify({'error': f'Agent {agent_name} is already running'}), 409
        
        # Start agent in background using SD framework approach
        try:
            # Use nohup to start agent in background with logging
            log_file = f'/tmp/{agent_name}.log'
            result = subprocess.Popen([
                'nohup', agent_path, '-d'
            ], stdout=open(log_file, 'w'), stderr=subprocess.STDOUT)
            
            # Give it a moment to start
            import time
            time.sleep(1)
            
            # Check if it started successfully
            check_result = subprocess.run(['pgrep', '-f', agent_name], 
                                        capture_output=True, text=True, timeout=5)
            if check_result.returncode == 0:
                return jsonify({'message': f'Started {agent_name} process (PID: {check_result.stdout.strip()})'})
            else:
                return jsonify({'error': f'Failed to start {agent_name} - check {log_file} for details'}), 500
                
        except Exception as e:
            return jsonify({'error': f'Failed to start agent: {e}'}), 500
            
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/api/agents/stop', methods=['POST'])
def stop_agent():
    """Stop an agent process"""
    try:
        data = request.get_json()
        agent_name = data.get('name')
        
        if not agent_name:
            return jsonify({'error': 'Agent name required'}), 400
        
        import subprocess
        import signal
        
        # Find the process PID using pgrep
        try:
            result = subprocess.run(['pgrep', '-f', agent_name], 
                                  capture_output=True, text=True, timeout=5)
            if result.returncode != 0 or not result.stdout.strip():
                return jsonify({'error': f'Agent {agent_name} is not running'}), 404
            
            # Get all PIDs (in case multiple processes)
            pids = [int(pid.strip()) for pid in result.stdout.strip().split('\n') if pid.strip()]
            
            # Try graceful shutdown with SIGTERM first
            stopped_pids = []
            for pid in pids:
                try:
                    import os
                    os.kill(pid, signal.SIGTERM)
                    stopped_pids.append(str(pid))
                except ProcessLookupError:
                    # Process already gone
                    pass
                except Exception as e:
                    print(f"Error stopping PID {pid}: {e}")
            
            if stopped_pids:
                # Give processes time to shutdown gracefully
                import time
                time.sleep(2)
                
                # Check if any are still running and force kill if needed
                still_running = []
                for pid in pids:
                    try:
                        os.kill(pid, 0)  # Just check if process exists
                        still_running.append(pid)
                    except ProcessLookupError:
                        # Process is gone, good
                        pass
                
                # Force kill any remaining processes
                for pid in still_running:
                    try:
                        os.kill(pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                
                return jsonify({'message': f'Stopped {agent_name} process(es) (PIDs: {", ".join(stopped_pids)})'})
            else:
                return jsonify({'error': f'Failed to stop {agent_name}'}), 500
                
        except Exception as e:
            return jsonify({'error': f'Failed to stop agent: {e}'}), 500
            
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/api/agents/restart', methods=['POST'])
def restart_agent():
    """Restart an agent process"""
    try:
        data = request.get_json()
        agent_name = data.get('name')
        
        if not agent_name:
            return jsonify({'error': 'Agent name required'}), 400
        
        import subprocess
        import signal
        import time
        
        # Get agent path from managed agents list
        managed_data = read_json_file(CONFIG['managed_agents_file'], {'agents': []})
        agent_path = None
        for agent in managed_data.get('agents', []):
            if agent['name'] == agent_name:
                agent_path = agent['path']
                break
        
        if not agent_path:
            return jsonify({'error': 'Agent path not found'}), 404
        
        # Stop existing process if running
        try:
            result = subprocess.run(['pgrep', '-f', agent_name], 
                                  capture_output=True, text=True, timeout=5)
            if result.returncode == 0 and result.stdout.strip():
                # Stop the agent first
                pids = [int(pid.strip()) for pid in result.stdout.strip().split('\n') if pid.strip()]
                for pid in pids:
                    try:
                        import os
                        os.kill(pid, signal.SIGTERM)
                    except ProcessLookupError:
                        pass
                
                # Wait for graceful shutdown
                time.sleep(2)
                
                # Force kill if still running
                for pid in pids:
                    try:
                        os.kill(pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
        except Exception:
            pass  # Continue with restart even if stop failed
        
        # Start the agent
        try:
            log_file = f'/tmp/{agent_name}.log'
            result = subprocess.Popen([
                'nohup', agent_path, '-d'
            ], stdout=open(log_file, 'w'), stderr=subprocess.STDOUT)
            
            # Give it a moment to start
            time.sleep(1)
            
            # Check if it started successfully
            check_result = subprocess.run(['pgrep', '-f', agent_name], 
                                        capture_output=True, text=True, timeout=5)
            if check_result.returncode == 0:
                return jsonify({'message': f'Restarted {agent_name} process (PID: {check_result.stdout.strip()})'})
            else:
                return jsonify({'error': f'Failed to restart {agent_name} - check {log_file} for details'}), 500
                
        except Exception as e:
            return jsonify({'error': f'Failed to restart agent: {e}'}), 500
            
    except Exception as e:
        return jsonify({'error': str(e)}), 500

if __name__ == '__main__':
    print("Service Controller Web Interface")
    print("================================")
    print(f"Data file: {CONFIG['data_file']}")
    print(f"Managed agents file: {CONFIG['managed_agents_file']}")
    print(f"Agent directory: {CONFIG['agent_dir']}")
    print(f"Relaxed validation: {CONFIG['relaxed_validation']}")
    print("")
    print("Note: This UI reads data files generated by the SC binary.")
    print("No direct SC binary invocation - clean separation of concerns.")
    print("")
    print("Dashboard available at: http://localhost:5001")
    print("Press Ctrl+C to stop")
    print("")
    
    app.run(host='127.0.0.1', port=5001, debug=False)