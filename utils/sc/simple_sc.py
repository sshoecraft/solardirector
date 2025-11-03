#!/usr/bin/env python3
"""
Simple Service Controller Data Generator
Generates JSON data for the web UI without complex C/JS integration
"""

import json
import os
import subprocess
import time
import signal
import sys
from datetime import datetime

CONFIG = {
    'managed_agents_file': '/tmp/sc_managed_agents.json',
    'data_export_file': '/tmp/sc_data.json',
    'discovery_interval': 60,
    'update_interval': 5
}

class SimpleServiceController:
    def __init__(self):
        self.agents = {}
        self.managed_agents = []
        self.last_discovery = 0
        self.running = True
        
        # Set up signal handlers
        signal.signal(signal.SIGINT, self.signal_handler)
        signal.signal(signal.SIGTERM, self.signal_handler)
    
    def signal_handler(self, signum, frame):
        print("\nShutting down SC data generator...")
        self.running = False
    
    def load_managed_agents(self):
        """Load managed agents from JSON file"""
        try:
            if os.path.exists(CONFIG['managed_agents_file']):
                with open(CONFIG['managed_agents_file'], 'r') as f:
                    data = json.load(f)
                    self.managed_agents = data.get('agents', [])
                    print(f"Loaded {len(self.managed_agents)} managed agents")
        except Exception as e:
            print(f"Error loading managed agents: {e}")
            self.managed_agents = []
    
    def get_agent_info(self, agent_path):
        """Get agent info by calling agent with -I flag"""
        try:
            result = subprocess.run([agent_path, '-I'], 
                                  capture_output=True, 
                                  text=True, 
                                  timeout=10)
            
            if result.returncode == 0 and result.stdout:
                return json.loads(result.stdout)
        except subprocess.TimeoutExpired:
            print(f"Timeout getting info for {agent_path}")
        except subprocess.CalledProcessError:
            print(f"Error calling {agent_path} -I")
        except json.JSONDecodeError:
            print(f"Invalid JSON from {agent_path}")
        except Exception as e:
            print(f"Error getting agent info for {agent_path}: {e}")
        
        return None
    
    def is_agent_running(self, agent_name):
        """Check if agent process is running"""
        try:
            # Use pgrep to find running processes by name
            result = subprocess.run(['pgrep', '-f', agent_name], 
                                  capture_output=True, 
                                  text=True, 
                                  timeout=5)
            return result.returncode == 0 and result.stdout.strip()
        except Exception:
            return False
    
    def get_agent_service_status(self, agent_name):
        """Check systemd service status if available"""
        try:
            result = subprocess.run(['systemctl', 'is-active', f'{agent_name}.service'], 
                                  capture_output=True, 
                                  text=True, 
                                  timeout=5)
            if result.returncode == 0:
                return result.stdout.strip()
        except Exception:
            pass
        return "unknown"
    
    def discover_agents(self):
        """Discover agents from managed list"""
        if time.time() - self.last_discovery < CONFIG['discovery_interval']:
            return
        
        print("Starting agent discovery...")
        self.last_discovery = time.time()
        
        # Clear existing agents
        self.agents = {}
        
        # Process each managed agent
        for managed in self.managed_agents:
            if not managed.get('enabled', True):
                continue
                
            name = managed['name']
            path = managed['path']
            
            print(f"Discovering agent: {name} at {path}")
            
            try:
                info_json = self.get_agent_info(path)
                if info_json:
                    # Check if agent is running
                    is_running = self.is_agent_running(name)
                    service_status = self.get_agent_service_status(name)
                    
                    # Determine status and health
                    if is_running:
                        status = 'Online'
                        health = 'Healthy'
                        online = True
                    else:
                        status = 'Offline'
                        health = 'Offline'
                        online = False
                    
                    agent = {
                        'name': name,
                        'path': path,
                        'role': info_json.get('agent_role', 'Unknown'),
                        'version': info_json.get('agent_version', 'Unknown'),
                        'status': status,
                        'online': online,
                        'service_status': service_status,
                        'have_status': is_running,
                        'have_data': is_running,
                        'last_seen': int(time.time()) if is_running else 0,
                        'last_status': int(time.time()) if is_running else 0,
                        'last_data': int(time.time()) if is_running else 0,
                        'current_time': int(time.time()),
                        'last_seen_ago': 'Now' if is_running else 'Never',
                        'last_status_ago': 'Now' if is_running else 'Never', 
                        'last_data_ago': 'Now' if is_running else 'Never',
                        'health': health,
                        'info_data': info_json
                    }
                    
                    self.agents[name] = agent
                    print(f"Added agent {name} (role: {agent['role']}, version: {agent['version']})")
                else:
                    print(f"Failed to get info for agent: {name}")
            except Exception as e:
                print(f"Error discovering agent {name}: {e}")
        
        print(f"Discovery complete, found {len(self.agents)} agents")
    
    def export_data(self):
        """Export current state to JSON file"""
        try:
            current_time = int(time.time())
            agent_list = []
            role_counts = {
                'inverter': 0,
                'battery': 0,
                'controller': 0,
                'storage': 0,
                'sensor': 0,
                'utility': 0,
                'device': 0,
                'other': 0
            }
            
            online_count = 0
            offline_count = 0
            
            # Process each agent
            for name, agent in self.agents.items():
                # Update time fields
                agent['current_time'] = current_time
                
                # Count by role
                role_key = agent['role'].lower()
                if role_key in role_counts:
                    role_counts[role_key] += 1
                else:
                    role_counts['other'] += 1
                
                # Update online/offline counts
                if agent['online']:
                    online_count += 1
                else:
                    offline_count += 1
                    
                agent_list.append(agent)
            
            # Create export data
            export_data = {
                'agents': {
                    'agents': agent_list
                },
                'system': {
                    'timestamp': current_time,
                    'total_agents': len(agent_list),
                    'online_agents': online_count,
                    'offline_agents': offline_count,
                    'last_discovery': int(self.last_discovery),
                    'discovery_interval': CONFIG['discovery_interval'],
                    'discovering': False,
                    'agent_dir': '/Users/steve/bin',
                    'role_counts': role_counts
                }
            }
            
            # Write to export file
            with open(CONFIG['data_export_file'], 'w') as f:
                json.dump(export_data, f, indent=4)
                
        except Exception as e:
            print(f"Error exporting data: {e}")
    
    def run(self):
        """Main execution loop"""
        print("Simple Service Controller Data Generator")
        print("=======================================")
        print(f"Managed agents file: {CONFIG['managed_agents_file']}")
        print(f"Data export file: {CONFIG['data_export_file']}")
        print(f"Discovery interval: {CONFIG['discovery_interval']}s")
        print(f"Update interval: {CONFIG['update_interval']}s")
        print()
        
        # Initial load
        self.load_managed_agents()
        self.discover_agents()
        self.export_data()
        
        # Main loop
        while self.running:
            try:
                # Check for changes to managed agents file
                self.load_managed_agents()
                
                # Periodic discovery
                self.discover_agents()
                
                # Export data
                self.export_data()
                
                # Sleep until next update
                time.sleep(CONFIG['update_interval'])
                
            except KeyboardInterrupt:
                break
            except Exception as e:
                print(f"Error in main loop: {e}")
                time.sleep(CONFIG['update_interval'])
        
        print("Shutdown complete")

if __name__ == '__main__':
    sc = SimpleServiceController()
    sc.run()