#!/usr/bin/env python3
"""
Flask Web Interface for Service Controller (SC)
"""

import os
import sys
import json
import time
import subprocess
import signal
from datetime import datetime
from pathlib import Path

from flask import Flask, render_template, jsonify, request, send_from_directory
# from flask_cors import CORS  # Temporarily disabled
import psutil

# SD config file reading functions
def read_sd_config_value(config_path, key):
    """Read a value from an SD config file"""
    try:
        with open(config_path, 'r') as f:
            in_global_section = True  # Start assuming we're in global section
            seen_section_header = False
            for line in f:
                line = line.strip()
                
                # Skip empty lines and comments
                if not line or line.startswith('#') or line.startswith(';'):
                    continue
                
                # Check for section headers
                if line.startswith('[') and line.endswith(']'):
                    seen_section_header = True
                    section = line[1:-1].strip()
                    in_global_section = (section == '' or section == 'global')
                    continue
                
                # If we haven't seen any section headers, assume we're in global section
                if not seen_section_header:
                    in_global_section = True
                
                # Only process lines in global section
                if not in_global_section and '=' in line:
                    continue
                
                # Look for key=value pairs
                if '=' in line:
                    config_key, config_value = line.split('=', 1)
                    config_key = config_key.strip()
                    config_value = config_value.strip()
                    
                    if config_key == key:
                        return config_value
        
        return None
    except (IOError, FileNotFoundError):
        return None

def get_agent_dir_from_config():
    """Get agent directory from SD config files with fallback"""
    config_files = []
    
    # Add home config file
    home_dir = os.path.expanduser('~')
    home_config = os.path.join(home_dir, '.sd.conf')
    if os.path.exists(home_config):
        config_files.append(home_config)
    
    # Add system config files
    system_configs = ['/etc/sd.conf', '/usr/local/etc/sd.conf']
    for config_file in system_configs:
        if os.path.exists(config_file):
            config_files.append(config_file)
    
    # Try each config file in order
    for config_file in config_files:
        # First try to get bindir directly
        bindir = read_sd_config_value(config_file, 'bindir')
        if bindir:
            return bindir
        
        # If no bindir, try prefix and append /bin
        prefix = read_sd_config_value(config_file, 'prefix')
        if prefix:
            agent_dir = f"{prefix}/bin"
            return agent_dir
    
    return None

def get_agent_directory():
    """Get agent directory with full fallback hierarchy"""
    # 1. Check SOLARD_BINDIR environment variable
    env_bindir = os.environ.get('SOLARD_BINDIR')
    if env_bindir:
        return env_bindir
    
    # 2. Check SD config files
    config_dir = get_agent_dir_from_config()
    if config_dir:
        return config_dir
    
    # 3. Check common default locations
    default_locations = ['/opt/sd/bin', '/usr/local/bin', '/Users/steve/src/sd/agents']
    for location in default_locations:
        if os.path.exists(location) and os.access(location, os.R_OK):
            return location
    
    # 4. Fall back to current directory
    current_dir = os.getcwd()
    return current_dir

# Flask application setup
app = Flask(__name__)
# CORS(app)  # Temporarily disabled

# Configuration
app.config['SECRET_KEY'] = os.environ.get('SECRET_KEY', 'dev-key-change-in-production')
app.config['SC_DATA_FILE'] = os.environ.get('SC_DATA_FILE', '/tmp/sc_data.json')
app.config['SC_BINARY_PATH'] = os.environ.get('SC_BINARY_PATH', '../sc')
app.config['AGENT_DIR'] = get_agent_directory()
app.config['RELAXED_VALIDATION'] = os.environ.get('SC_RELAXED_VALIDATION', 'true').lower() == 'true'

# Global variables for background SC process
sc_process = None
sc_data = {'agents': [], 'system': {}, 'last_update': 0}

def get_sc_data():
    """Read the latest SC data from JSON file"""
    global sc_data
    
    try:
        data_file = app.config['SC_DATA_FILE']
        if os.path.exists(data_file):
            with open(data_file, 'r') as f:
                data = json.load(f)
                if data:
                    sc_data = data
                    sc_data['last_update'] = time.time()
                    return sc_data
    except (json.JSONDecodeError, FileNotFoundError, IOError) as e:
        app.logger.warning(f"Failed to read SC data file: {e}")
    
    return sc_data

def start_sc_process():
    """Start the SC process in web export mode"""
    global sc_process
    
    try:
        sc_binary = Path(app.config['SC_BINARY_PATH']).resolve()
        if not sc_binary.exists():
            print(f"ERROR: SC binary not found: {sc_binary}")
            return False
            
        print(f"Starting SC process: {sc_binary}")
        
        # Start SC in web export mode
        cmd = [str(sc_binary), '-w', '-r', '5']  # 5 second refresh rate
        print(f"Command: {' '.join(cmd)}")
        
        sc_process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            cwd=sc_binary.parent,  # Set working directory
            preexec_fn=os.setsid  # Create new process group
        )
        
        print(f"Started SC process with PID: {sc_process.pid}")
        
        # Give it a moment to start
        time.sleep(1)
        
        # Check if process is still running
        if sc_process.poll() is None:
            print("SC process is running successfully")
            return True
        else:
            stdout, stderr = sc_process.communicate()
            print(f"SC process exited with code: {sc_process.returncode}")
            print(f"SC stdout: {stdout.decode()}")
            print(f"SC stderr: {stderr.decode()}")
            return False
        
    except Exception as e:
        print(f"Failed to start SC process: {e}")
        import traceback
        traceback.print_exc()
        return False

def stop_sc_process():
    """Stop the SC process"""
    global sc_process
    
    if sc_process and sc_process.poll() is None:
        try:
            # Terminate the process group
            os.killpg(os.getpgid(sc_process.pid), signal.SIGTERM)
            sc_process.wait(timeout=5)
            app.logger.info("SC process stopped")
        except (ProcessLookupError, subprocess.TimeoutExpired):
            # Force kill if graceful termination fails
            try:
                os.killpg(os.getpgid(sc_process.pid), signal.SIGKILL)
                app.logger.warning("SC process force killed")
            except ProcessLookupError:
                pass
        finally:
            sc_process = None

def manage_agent_service(agent_name, action):
    """Manage systemd service for an agent"""
    valid_actions = ['start', 'stop', 'restart', 'enable', 'disable', 'status']
    if action not in valid_actions:
        return {'success': False, 'error': f'Invalid action: {action}'}
    
    service_name = f"{agent_name}.service"
    
    try:
        # Check if service exists
        check_cmd = ['systemctl', 'list-unit-files', service_name]
        result = subprocess.run(check_cmd, capture_output=True, text=True, timeout=10)
        
        if service_name not in result.stdout:
            return {'success': False, 'error': f'Service {service_name} not found'}
        
        # Execute the action
        if action == 'status':
            cmd = ['systemctl', 'is-active', service_name]
        else:
            cmd = ['sudo', 'systemctl', action, service_name]
        
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        
        if action == 'status':
            status = result.stdout.strip()
            return {'success': True, 'status': status, 'active': status == 'active'}
        else:
            success = result.returncode == 0
            return {
                'success': success,
                'message': f"Agent {agent_name} {action} {'successful' if success else 'failed'}",
                'output': result.stdout.strip(),
                'error': result.stderr.strip() if result.stderr else None
            }
            
    except subprocess.TimeoutExpired:
        return {'success': False, 'error': f'Operation timeout for {action} on {agent_name}'}
    except Exception as e:
        return {'success': False, 'error': f'Error managing agent {agent_name}: {str(e)}'}

# Routes

@app.route('/')
def index():
    """Main dashboard page"""
    return render_template('dashboard.html')

@app.route('/test')
def test_page():
    """Test page for debugging API issues"""
    return send_from_directory('.', 'test_api.html')

@app.route('/agent/<agent_name>')
def agent_detail(agent_name):
    """Agent detail page"""
    return render_template('agent.html', agent_name=agent_name)

# API Routes

@app.route('/api/agents')
def api_agents():
    """Get all agents with current status"""
    data = get_sc_data()
    agents_data = data.get('agents', {}).get('agents', [])
    
    # Add service status for each agent
    for agent in agents_data:
        service_status = manage_agent_service(agent['name'], 'status')
        agent['service_active'] = service_status.get('active', False)
        agent['service_status'] = service_status.get('status', 'unknown')
    
    return jsonify({
        'agents': agents_data,
        'count': len(agents_data),
        'last_update': data.get('last_update', 0)
    })

@app.route('/api/agents/<agent_name>')
def api_agent_detail(agent_name):
    """Get specific agent details"""
    data = get_sc_data()
    agents_data = data.get('agents', {}).get('agents', [])
    
    agent = next((a for a in agents_data if a['name'] == agent_name), None)
    if not agent:
        return jsonify({'error': 'Agent not found'}), 404
    
    # Add service status
    service_status = manage_agent_service(agent_name, 'status')
    agent['service_active'] = service_status.get('active', False)
    agent['service_status'] = service_status.get('status', 'unknown')
    
    return jsonify(agent)

@app.route('/api/agents/<agent_name>/status')
def api_agent_status(agent_name):
    """Get agent status only"""
    data = get_sc_data()
    agents_data = data.get('agents', {}).get('agents', [])
    
    agent = next((a for a in agents_data if a['name'] == agent_name), None)
    if not agent:
        return jsonify({'error': 'Agent not found'}), 404
    
    service_status = manage_agent_service(agent_name, 'status')
    
    return jsonify({
        'name': agent['name'],
        'status': agent.get('status', 'Unknown'),
        'health': agent.get('health', 'Unknown'),
        'online': agent.get('online', False),
        'service_active': service_status.get('active', False),
        'service_status': service_status.get('status', 'unknown'),
        'last_seen': agent.get('last_seen', 0),
        'last_seen_ago': agent.get('last_seen_ago', 'Never')
    })

@app.route('/api/agents/<agent_name>/<action>', methods=['POST'])
def api_agent_action(agent_name, action):
    """Perform action on agent (start/stop/restart/enable/disable)"""
    data = get_sc_data()
    agents_data = data.get('agents', {}).get('agents', [])
    
    agent = next((a for a in agents_data if a['name'] == agent_name), None)
    if not agent:
        return jsonify({'error': 'Agent not found'}), 404
    
    result = manage_agent_service(agent_name, action)
    
    if result['success']:
        return jsonify(result)
    else:
        return jsonify(result), 400

@app.route('/api/system/status')
def api_system_status():
    """Get overall system status"""
    data = get_sc_data()
    system_data = data.get('system', {})
    
    # Add Flask server info
    system_data['flask_server'] = {
        'uptime': time.time() - app.start_time,
        'pid': os.getpid(),
        'sc_process_running': sc_process is not None and sc_process.poll() is None
    }
    
    # Add system resource info
    try:
        system_data['resources'] = {
            'cpu_percent': psutil.cpu_percent(interval=1),
            'memory_percent': psutil.virtual_memory().percent,
            'disk_percent': psutil.disk_usage('/').percent if os.path.exists('/') else 0
        }
    except Exception as e:
        app.logger.warning(f"Failed to get system resources: {e}")
    
    return jsonify(system_data)

@app.route('/api/system/restart-sc', methods=['POST'])
def api_restart_sc():
    """Restart the SC background process"""
    stop_sc_process()
    time.sleep(1)
    
    if start_sc_process():
        return jsonify({'success': True, 'message': 'SC process restarted'})
    else:
        return jsonify({'success': False, 'error': 'Failed to restart SC process'}), 500

@app.route('/api/available-agents')
def api_available_agents():
    """Get list of agents in SOLARD_BINDIR that are not yet managed"""
    try:
        # Get available agents by calling SC with a special mode
        sc_binary = Path(app.config['SC_BINARY_PATH']).resolve()
        if not sc_binary.exists():
            app.logger.error(f"SC binary not found: {sc_binary}")
            return jsonify({'error': 'SC binary not found'}), 500
        
        # Create a temporary JSON file to get available agents data
        temp_file = '/tmp/sc_available_agents.json'
        
        # We'll need to add an export mode for available agents in the C code
        # For now, let's simulate by scanning the directory
        agent_dir = app.config['AGENT_DIR']
        available_agents = []
        relaxed_mode = app.config['RELAXED_VALIDATION']
        
        app.logger.info(f"Scanning for available agents in: {agent_dir}")
        
        if os.path.exists(agent_dir):
            # Get managed agents from the same file used by add/remove endpoints
            managed_agents_file = '/tmp/sc_managed_agents.json'
            managed_agents_list = []
            if os.path.exists(managed_agents_file):
                try:
                    with open(managed_agents_file, 'r') as f:
                        managed_data = json.load(f)
                        managed_agents_list = [a['name'] for a in managed_data.get('agents', [])]
                except (json.JSONDecodeError, IOError):
                    pass
            managed_agents = managed_agents_list
            
            all_items = os.listdir(agent_dir)
            app.logger.info(f"Found {len(all_items)} items to check, {len(managed_agents)} already managed")
            
            # Scan for agents - handle both source directories and direct binaries
            for item in all_items:
                item_path = os.path.join(agent_dir, item)
                
                if os.path.isdir(item_path) and not item.startswith('.'):
                    # Check if this looks like an agent source directory
                    makefile_path = os.path.join(item_path, 'Makefile')
                    main_c_path = os.path.join(item_path, 'main.c')
                    binary_path = os.path.join(item_path, item)
                    
                    if os.path.exists(makefile_path) and os.path.exists(main_c_path):
                        # Skip if already managed
                        if item in managed_agents:
                            continue
                            
                        # Check if there's a compiled binary
                        if os.path.exists(binary_path) and os.access(binary_path, os.X_OK):
                            agent_info = {
                                'name': item,
                                'path': binary_path,
                                'role': 'unknown',
                                'version': 'unknown',
                                'compiled': True,
                                'validation_failed': False,
                                'validation_warning': None
                            }
                            
                            # Try to get agent info by running with -I flag
                            try:
                                result = subprocess.run([binary_path, '-I'], 
                                                      capture_output=True, text=True, timeout=5)
                                if result.returncode == 0 and result.stdout:
                                    # Parse JSON output to get agent info
                                    try:
                                        parsed_info = json.loads(result.stdout)
                                        agent_name = parsed_info.get('agent_name')
                                        if agent_name:
                                            agent_info.update({
                                                'name': agent_name,
                                                'role': parsed_info.get('agent_role', 'unknown'),
                                                'version': parsed_info.get('agent_version', 'unknown')
                                            })
                                    except json.JSONDecodeError as e:
                                        if relaxed_mode:
                                            agent_info['validation_failed'] = True
                                            agent_info['validation_warning'] = f'Failed to parse -I output as JSON: {str(e)}'
                                        else:
                                            continue  # Skip if strict validation
                                else:
                                    if relaxed_mode:
                                        agent_info['validation_failed'] = True
                                        agent_info['validation_warning'] = f'Agent returned non-zero exit code ({result.returncode}) or no output'
                                        if result.stderr:
                                            agent_info['validation_warning'] += f': {result.stderr.strip()}'
                                    else:
                                        continue  # Skip if strict validation
                            except (subprocess.TimeoutExpired, subprocess.SubprocessError) as e:
                                if relaxed_mode:
                                    agent_info['validation_failed'] = True
                                    agent_info['validation_warning'] = f'Agent -I test failed: {str(e)}'
                                else:
                                    continue  # Skip if strict validation
                            
                            available_agents.append(agent_info)
                        else:
                            # If no working compiled binary, offer the uncompiled version
                            available_agents.append({
                                'name': item,
                                'path': binary_path,
                                'role': 'unknown',
                                'version': 'uncompiled',
                                'needs_build': True,
                                'compiled': False,
                                'validation_failed': False,
                                'validation_warning': 'Binary not found or not executable - needs compilation'
                            })
                            
                elif os.path.isfile(item_path) and os.access(item_path, os.X_OK) and not item.startswith('.'):
                    # Check if this is a direct agent binary (not in a source directory)
                    
                    # Skip common non-agent binaries and utilities
                    skip_binaries = {
                        'code', 'free', 'mem', 'notify', 'hermit', 'hermit-stable', 'claude-shell.py',
                        'cellmon', 'cellmon.bin',  # Cell monitoring utilities
                        'sdconfig', 'sdjs', 'rdevserver', 'siutil', 'sunrise', 'sunset', 'watchsi'  # SD utilities
                    }
                    if item in skip_binaries:
                        continue
                        
                    # Skip if already managed
                    if item in managed_agents:
                        continue
                    
                    agent_info = {
                        'name': item,
                        'path': item_path,
                        'role': 'unknown',
                        'version': 'unknown',
                        'compiled': True,
                        'validation_failed': False,
                        'validation_warning': None,
                        'direct_binary': True  # Mark as direct binary
                    }
                    
                    # Try to get agent info by running with -I flag
                    try:
                        result = subprocess.run([item_path, '-I'], 
                                              capture_output=True, text=True, timeout=5)
                        if result.returncode == 0 and result.stdout:
                            # Parse JSON output to get agent info
                            try:
                                parsed_info = json.loads(result.stdout)
                                agent_name = parsed_info.get('agent_name')
                                if agent_name:
                                    agent_info.update({
                                        'name': agent_name,
                                        'role': parsed_info.get('agent_role', 'unknown'),
                                        'version': parsed_info.get('agent_version', 'unknown')
                                    })
                            except json.JSONDecodeError as e:
                                if relaxed_mode:
                                    agent_info['validation_failed'] = True
                                    agent_info['validation_warning'] = f'Failed to parse -I output as JSON: {str(e)}'
                                else:
                                    continue  # Skip if strict validation
                        else:
                            if relaxed_mode:
                                agent_info['validation_failed'] = True
                                agent_info['validation_warning'] = f'Agent returned non-zero exit code ({result.returncode}) or no output'
                                if result.stderr:
                                    agent_info['validation_warning'] += f': {result.stderr.strip()}'
                            else:
                                continue  # Skip if strict validation
                    except (subprocess.TimeoutExpired, subprocess.SubprocessError) as e:
                        if relaxed_mode:
                            agent_info['validation_failed'] = True
                            agent_info['validation_warning'] = f'Agent -I test failed: {str(e)}'
                        else:
                            continue  # Skip if strict validation
                    
                    available_agents.append(agent_info)
        
        app.logger.info(f"Agent discovery complete. Found {len(available_agents)} available agents")
        
        return jsonify({
            'available_agents': available_agents,
            'count': len(available_agents),
            'agent_dir': agent_dir,
            'relaxed_mode': relaxed_mode,
            'managed_agents': managed_agents,
            'debug_info': f"Found {len(managed_agents)} managed agents, scanned {len(all_items)} items"
        })
        
    except Exception as e:
        app.logger.error(f"Failed to get available agents: {e}")
        return jsonify({'error': f'Failed to get available agents: {str(e)}'}), 500

@app.route('/api/agents/add', methods=['POST'])
def api_add_agent():
    """Add an agent to monitoring"""
    try:
        data = request.get_json()
        if not data:
            return jsonify({'error': 'No data provided'}), 400
        
        agent_name = data.get('name')
        agent_path = data.get('path')
        force_add = data.get('force', False)  # Allow forcing add even with validation warnings
        
        if not agent_name or not agent_path:
            return jsonify({'error': 'Agent name and path are required'}), 400
        
        # Validate the agent path exists and is executable
        if not os.path.exists(agent_path):
            return jsonify({'error': f'Agent path does not exist: {agent_path}'}), 400
        
        if not os.access(agent_path, os.X_OK):
            return jsonify({'error': f'Agent path is not executable: {agent_path}'}), 400
        
        relaxed_mode = app.config['RELAXED_VALIDATION']
        validation_warnings = []
        
        # Try to get agent info to validate it's a proper agent
        if not force_add:  # Skip validation if forcing
            try:
                result = subprocess.run([agent_path, '-I'], 
                                      capture_output=True, text=True, timeout=5)
                if result.returncode != 0:
                    error_msg = f'Agent returned non-zero exit code ({result.returncode})'
                    if result.stderr:
                        error_msg += f': {result.stderr.strip()}'
                    
                    if relaxed_mode:
                        validation_warnings.append(error_msg)
                    else:
                        return jsonify({'error': f'Invalid agent: {agent_path} does not respond to -I flag - {error_msg}'}), 400
                else:
                    # Parse the response to validate it's proper JSON
                    try:
                        agent_info = json.loads(result.stdout)
                        if not agent_info.get('agent_name'):
                            warning_msg = f'Agent does not provide agent_name in -I output'
                            if relaxed_mode:
                                validation_warnings.append(warning_msg)
                            else:
                                return jsonify({'error': f'Invalid agent: {agent_path} - {warning_msg}'}), 400
                    except json.JSONDecodeError as e:
                        warning_msg = f'Failed to parse -I output as JSON: {str(e)}'
                        if relaxed_mode:
                            validation_warnings.append(warning_msg)
                        else:
                            return jsonify({'error': f'Invalid agent: {agent_path} - {warning_msg}'}), 400
                    
            except (subprocess.TimeoutExpired, subprocess.SubprocessError) as e:
                error_msg = f'Agent -I test failed: {str(e)}'
                if relaxed_mode:
                    validation_warnings.append(error_msg)
                else:
                    return jsonify({'error': f'Failed to validate agent: {error_msg}'}), 400
        
        # For now, we'll add it to a simple managed agents file
        # In a full implementation, this would call the SC C function
        managed_agents_file = '/tmp/sc_managed_agents.json'
        managed_agents = {'agents': []}
        
        # Load existing managed agents
        if os.path.exists(managed_agents_file):
            try:
                with open(managed_agents_file, 'r') as f:
                    managed_agents = json.load(f)
            except (json.JSONDecodeError, IOError):
                pass
        
        # Check if agent already exists
        for agent in managed_agents.get('agents', []):
            if agent.get('name') == agent_name:
                return jsonify({'error': f'Agent {agent_name} is already managed'}), 400
        
        # Add the new agent
        new_agent = {
            'name': agent_name,
            'path': agent_path,
            'enabled': True,
            'added': time.time(),
            'validation_warnings': validation_warnings if validation_warnings else None
        }
        managed_agents.setdefault('agents', []).append(new_agent)
        
        # Save managed agents
        with open(managed_agents_file, 'w') as f:
            json.dump(managed_agents, f, indent=2)
        
        # Restart SC process to pick up the new agent
        stop_sc_process()
        time.sleep(1)
        start_sc_process()
        
        response = {
            'success': True,
            'message': f'Agent {agent_name} added successfully',
            'agent': new_agent
        }
        
        if validation_warnings:
            response['warnings'] = validation_warnings
            response['message'] += f' (with {len(validation_warnings)} validation warning(s))'
        
        return jsonify(response)
        
    except Exception as e:
        app.logger.error(f"Failed to add agent: {e}")
        return jsonify({'error': f'Failed to add agent: {str(e)}'}), 500

@app.route('/api/agents/<agent_name>/remove', methods=['DELETE'])
def api_remove_agent(agent_name):
    """Remove an agent from monitoring"""
    try:
        # Load managed agents
        managed_agents_file = '/tmp/sc_managed_agents.json'
        managed_agents = {'agents': []}
        
        if os.path.exists(managed_agents_file):
            try:
                with open(managed_agents_file, 'r') as f:
                    managed_agents = json.load(f)
            except (json.JSONDecodeError, IOError):
                pass
        
        # Find and remove the agent
        original_count = len(managed_agents.get('agents', []))
        managed_agents['agents'] = [
            agent for agent in managed_agents.get('agents', [])
            if agent.get('name') != agent_name
        ]
        
        if len(managed_agents['agents']) == original_count:
            return jsonify({'error': f'Agent {agent_name} not found'}), 404
        
        # Save managed agents
        with open(managed_agents_file, 'w') as f:
            json.dump(managed_agents, f, indent=2)
        
        # Restart SC process to remove the agent
        stop_sc_process()
        time.sleep(1)
        start_sc_process()
        
        return jsonify({
            'success': True,
            'message': f'Agent {agent_name} removed successfully'
        })
        
    except Exception as e:
        app.logger.error(f"Failed to remove agent: {e}")
        return jsonify({'error': f'Failed to remove agent: {str(e)}'}), 500

# Error handlers

@app.errorhandler(404)
def not_found(error):
    return jsonify({'error': 'Not found'}), 404

@app.errorhandler(500)
def internal_error(error):
    return jsonify({'error': 'Internal server error'}), 500

# Application lifecycle

def cleanup():
    """Cleanup function called on shutdown"""
    stop_sc_process()

# Register cleanup function
import atexit
atexit.register(cleanup)

if __name__ == '__main__':
    print("Initializing Service Controller Web Interface...")
    
    # Record start time
    app.start_time = time.time()
    
    # Setup environment
    print(f"SC Binary Path: {app.config['SC_BINARY_PATH']}")
    print(f"SC Data File: {app.config['SC_DATA_FILE']}")
    print(f"Agent Directory: {app.config['AGENT_DIR']}")
    print(f"Relaxed Validation: {app.config['RELAXED_VALIDATION']}")
    if app.config['RELAXED_VALIDATION']:
        print("Note: Relaxed validation is enabled - agents will be included even if they fail -I test")
    
    # Start SC process
    print("\nStarting SC background process...")
    if start_sc_process():
        print("SC process started successfully")
    else:
        print("Warning: Failed to start SC process - continuing without it")
        print("Agent data will not be available until SC process is fixed")
    
    # Wait a moment for SC to initialize
    time.sleep(2)
    
    try:
        # Run Flask app - Use port 5001 by default to avoid macOS AirPlay port conflict
        port = int(os.environ.get('PORT', 5001))
        debug = os.environ.get('FLASK_DEBUG', 'False').lower() == 'true'
        
        print(f"\nStarting Flask web interface...")
        print(f"Port: {port}")
        print(f"Debug mode: {debug}")
        print(f"Dashboard available at: http://localhost:{port}")
        print("Press Ctrl+C to stop the server\n")
        
        app.run(host='127.0.0.1', port=port, debug=debug, use_reloader=False)
        
    except KeyboardInterrupt:
        print("\nShutting down...")
    except Exception as e:
        print(f"Error starting Flask: {e}")
        import traceback
        traceback.print_exc()
    finally:
        cleanup()