#!/usr/bin/env python3
"""
C Backend Testing Script for Service Controller Agent Management
================================================================

This script tests the C backend functionality including:
- SOLARD_BINDIR environment variable handling
- Agent discovery and management functions
- JSON export functionality
- Integration with Flask API

Author: TESTER Agent
Date: 2025-07-13
"""

import os
import sys
import json
import time
import tempfile
import shutil
import subprocess
import signal
from pathlib import Path

class CBackendTester:
    """Tester for Service Controller C backend"""
    
    def __init__(self):
        self.test_dir = tempfile.mkdtemp(prefix='sc_c_test_')
        self.agent_dir = os.path.join(self.test_dir, 'agents')
        self.config_dir = os.path.join(self.test_dir, 'config')
        self.sc_binary = None
        self.setup_test_environment()
    
    def setup_test_environment(self):
        """Set up test environment"""
        print(f"Setting up C backend test environment: {self.test_dir}")
        
        # Create directories
        os.makedirs(self.agent_dir, exist_ok=True)
        os.makedirs(self.config_dir, exist_ok=True)
        
        # Create test agents
        self.create_test_agents()
        
        # Check if SC binary exists and is executable
        self.find_sc_binary()
        
        print("C backend test environment ready")
    
    def find_sc_binary(self):
        """Find the SC binary for testing"""
        possible_paths = [
            './sc',
            '../sc',
            '/opt/sd/bin/sc',
            './utils/sc/sc'
        ]
        
        for path in possible_paths:
            if os.path.exists(path) and os.access(path, os.X_OK):
                self.sc_binary = os.path.abspath(path)
                print(f"Found SC binary: {self.sc_binary}")
                return
        
        print("SC binary not found. Some tests may be skipped.")
    
    def create_test_agents(self):
        """Create test agents for C backend testing"""
        agents = [
            {'name': 'c_test_si', 'role': 'inverter', 'version': '1.0.0'},
            {'name': 'c_test_jbd', 'role': 'battery', 'version': '1.2.0'},
            {'name': 'c_test_ac', 'role': 'controller', 'version': '2.1.0'},
            {'name': 'c_test_sensor', 'role': 'sensor', 'version': '0.5.0'},
            {'name': 'c_invalid_agent', 'role': None, 'version': None}  # Invalid
        ]
        
        for agent in agents:
            agent_path = os.path.join(self.agent_dir, agent['name'])
            
            if agent['role']:
                # Valid agent
                script_content = f'''#!/bin/bash
if [ "$1" = "-I" ]; then
    echo '{{"agent_name": "{agent['name']}", "agent_role": "{agent['role']}", "agent_version": "{agent['version']}"}}'
    exit 0
else
    echo "Agent {agent['name']} running"
    sleep 60  # Simulate long-running agent
fi
'''
            else:
                # Invalid agent
                script_content = '''#!/bin/bash
echo "Invalid agent"
exit 1
'''
            
            with open(agent_path, 'w') as f:
                f.write(script_content)
            os.chmod(agent_path, 0o755)
        
        print(f"Created {len(agents)} test agents")
    
    def test_solard_bindir_functionality(self):
        """Test SOLARD_BINDIR environment variable functionality"""
        print("\n" + "="*60)
        print("Testing SOLARD_BINDIR Functionality")
        print("="*60)
        
        if not self.sc_binary:
            print("SKIP: SC binary not available")
            return
        
        # Test 1: With SOLARD_BINDIR set
        print("\nTest 1: SOLARD_BINDIR set to custom directory")
        env = os.environ.copy()
        env['SOLARD_BINDIR'] = self.agent_dir
        
        try:
            result = subprocess.run(
                [self.sc_binary, '-l'],
                env=env,
                capture_output=True,
                text=True,
                timeout=10
            )
            
            if result.returncode == 0:
                print("✓ SC -l executed successfully with custom SOLARD_BINDIR")
                print(f"Output: {result.stdout[:200]}...")
            else:
                print(f"✗ SC -l failed: {result.stderr}")
        except subprocess.TimeoutExpired:
            print("✗ SC -l timed out")
        except Exception as e:
            print(f"✗ SC -l error: {e}")
        
        # Test 2: Without SOLARD_BINDIR (should use default)
        print("\nTest 2: SOLARD_BINDIR not set (default behavior)")
        env = os.environ.copy()
        if 'SOLARD_BINDIR' in env:
            del env['SOLARD_BINDIR']
        
        try:
            result = subprocess.run(
                [self.sc_binary, '-l'],
                env=env,
                capture_output=True,
                text=True,
                timeout=10
            )
            
            if result.returncode == 0:
                print("✓ SC -l executed successfully with default SOLARD_BINDIR")
            else:
                print(f"✗ SC -l failed with default: {result.stderr}")
        except subprocess.TimeoutExpired:
            print("✗ SC -l with default timed out")
        except Exception as e:
            print(f"✗ SC -l with default error: {e}")
    
    def test_agent_discovery(self):
        """Test agent discovery functionality"""
        print("\n" + "="*60)
        print("Testing Agent Discovery")
        print("="*60)
        
        if not self.sc_binary:
            print("SKIP: SC binary not available")
            return
        
        env = os.environ.copy()
        env['SOLARD_BINDIR'] = self.agent_dir
        
        try:
            result = subprocess.run(
                [self.sc_binary, '-l'],
                env=env,
                capture_output=True,
                text=True,
                timeout=15
            )
            
            if result.returncode == 0:
                print("✓ Agent discovery completed")
                
                # Check output for our test agents
                output = result.stdout
                valid_agents = ['c_test_si', 'c_test_jbd', 'c_test_ac', 'c_test_sensor']
                
                found_agents = []
                for agent in valid_agents:
                    if agent in output:
                        found_agents.append(agent)
                        print(f"  ✓ Found agent: {agent}")
                    else:
                        print(f"  ✗ Missing agent: {agent}")
                
                if 'c_invalid_agent' in output:
                    print("  ✗ Invalid agent incorrectly discovered")
                else:
                    print("  ✓ Invalid agent correctly ignored")
                
                print(f"Total discovered: {len(found_agents)}/{len(valid_agents)}")
                
            else:
                print(f"✗ Agent discovery failed: {result.stderr}")
        
        except subprocess.TimeoutExpired:
            print("✗ Agent discovery timed out")
        except Exception as e:
            print(f"✗ Agent discovery error: {e}")
    
    def test_json_export(self):
        """Test JSON export functionality"""
        print("\n" + "="*60)
        print("Testing JSON Export")
        print("="*60)
        
        if not self.sc_binary:
            print("SKIP: SC binary not available")
            return
        
        export_file = os.path.join(self.test_dir, 'export_test.json')
        env = os.environ.copy()
        env['SOLARD_BINDIR'] = self.agent_dir
        
        try:
            result = subprocess.run(
                [self.sc_binary, '-e', export_file],
                env=env,
                capture_output=True,
                text=True,
                timeout=15
            )
            
            if result.returncode == 0:
                print("✓ JSON export completed")
                
                # Verify export file was created
                if os.path.exists(export_file):
                    print("✓ Export file created")
                    
                    # Verify JSON is valid
                    try:
                        with open(export_file, 'r') as f:
                            data = json.load(f)
                        
                        print("✓ Export file contains valid JSON")
                        
                        # Check structure
                        required_keys = ['agents', 'system']
                        for key in required_keys:
                            if key in data:
                                print(f"  ✓ Contains '{key}' section")
                            else:
                                print(f"  ✗ Missing '{key}' section")
                        
                        # Check agents data
                        if 'agents' in data and 'agents' in data['agents']:
                            agent_count = len(data['agents']['agents'])
                            print(f"  ✓ Contains {agent_count} agents")
                            
                            # Check agent structure
                            if agent_count > 0:
                                agent = data['agents']['agents'][0]
                                required_fields = ['name', 'role', 'version', 'path']
                                for field in required_fields:
                                    if field in agent:
                                        print(f"    ✓ Agent has '{field}' field")
                                    else:
                                        print(f"    ✗ Agent missing '{field}' field")
                    
                    except json.JSONDecodeError as e:
                        print(f"✗ Export file contains invalid JSON: {e}")
                    except Exception as e:
                        print(f"✗ Error reading export file: {e}")
                
                else:
                    print("✗ Export file was not created")
            
            else:
                print(f"✗ JSON export failed: {result.stderr}")
        
        except subprocess.TimeoutExpired:
            print("✗ JSON export timed out")
        except Exception as e:
            print(f"✗ JSON export error: {e}")
    
    def test_web_export_mode(self):
        """Test web export mode functionality"""
        print("\n" + "="*60)
        print("Testing Web Export Mode")
        print("="*60)
        
        if not self.sc_binary:
            print("SKIP: SC binary not available")
            return
        
        export_file = '/tmp/sc_data.json'
        env = os.environ.copy()
        env['SOLARD_BINDIR'] = self.agent_dir
        
        # Clean up any existing export file
        if os.path.exists(export_file):
            os.remove(export_file)
        
        print("Starting web export mode for 10 seconds...")
        
        try:
            # Start SC in web export mode
            process = subprocess.Popen(
                [self.sc_binary, '-w', '-r', '2'],  # 2 second refresh
                env=env,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                preexec_fn=os.setsid  # Create new process group
            )
            
            # Let it run for a few seconds
            time.sleep(6)
            
            # Check if export file is being created/updated
            file_checks = []
            for i in range(3):
                if os.path.exists(export_file):
                    mtime = os.path.getmtime(export_file)
                    file_checks.append(mtime)
                    print(f"  Export file exists at check {i+1}")
                else:
                    file_checks.append(None)
                    print(f"  Export file missing at check {i+1}")
                time.sleep(2)
            
            # Terminate the process
            os.killpg(os.getpgid(process.pid), signal.SIGTERM)
            process.wait(timeout=5)
            
            # Analyze results
            existing_files = [t for t in file_checks if t is not None]
            if len(existing_files) >= 2:
                if existing_files[-1] > existing_files[0]:
                    print("✓ Export file is being updated regularly")
                else:
                    print("✗ Export file not updating")
            elif len(existing_files) > 0:
                print("✓ Export file created")
            else:
                print("✗ Export file never created")
        
        except subprocess.TimeoutExpired:
            print("✗ Web export mode timed out")
        except Exception as e:
            print(f"✗ Web export mode error: {e}")
    
    def test_managed_agents_functionality(self):
        """Test managed agents file functionality"""
        print("\n" + "="*60)
        print("Testing Managed Agents Functionality")
        print("="*60)
        
        # Create a managed agents file
        managed_agents_file = os.path.join(self.test_dir, 'managed_agents.json')
        managed_data = {
            'agents': [
                {
                    'name': 'c_test_si',
                    'path': os.path.join(self.agent_dir, 'c_test_si'),
                    'enabled': True,
                    'added': time.time()
                },
                {
                    'name': 'c_test_jbd',
                    'path': os.path.join(self.agent_dir, 'c_test_jbd'),
                    'enabled': False,  # Disabled agent
                    'added': time.time()
                }
            ]
        }
        
        with open(managed_agents_file, 'w') as f:
            json.dump(managed_data, f, indent=2)
        
        print(f"✓ Created managed agents file: {managed_agents_file}")
        
        # Test if SC can load and process managed agents
        if self.sc_binary:
            env = os.environ.copy()
            env['SOLARD_BINDIR'] = self.agent_dir
            
            # Copy managed agents file to expected location
            expected_location = './managed_agents.json'
            shutil.copy(managed_agents_file, expected_location)
            
            try:
                result = subprocess.run(
                    [self.sc_binary, '-l'],
                    env=env,
                    capture_output=True,
                    text=True,
                    timeout=10,
                    cwd=self.test_dir
                )
                
                if result.returncode == 0:
                    output = result.stdout
                    
                    # Check if enabled agent appears
                    if 'c_test_si' in output:
                        print("✓ Enabled managed agent discovered")
                    else:
                        print("✗ Enabled managed agent not found")
                    
                    # Check if disabled agent is ignored
                    if 'c_test_jbd' not in output:
                        print("✓ Disabled managed agent correctly ignored")
                    else:
                        print("✗ Disabled managed agent incorrectly included")
                
                else:
                    print(f"✗ SC failed to process managed agents: {result.stderr}")
            
            except Exception as e:
                print(f"✗ Error testing managed agents: {e}")
            
            finally:
                # Clean up
                if os.path.exists(expected_location):
                    os.remove(expected_location)
        
        else:
            print("SKIP: SC binary not available for managed agents test")
    
    def test_configuration_integration(self):
        """Test configuration file integration"""
        print("\n" + "="*60)
        print("Testing Configuration Integration")
        print("="*60)
        
        # Create SC configuration file
        config_file = os.path.join(self.test_dir, 'sc.conf')
        config_content = f'''# SC Configuration
agent_dir = {self.agent_dir}
refresh_rate = 5
'''
        
        with open(config_file, 'w') as f:
            f.write(config_content)
        
        print(f"✓ Created configuration file: {config_file}")
        
        if self.sc_binary:
            env = os.environ.copy()
            # Don't set SOLARD_BINDIR - should use config file
            if 'SOLARD_BINDIR' in env:
                del env['SOLARD_BINDIR']
            
            try:
                result = subprocess.run(
                    [self.sc_binary, '-c', config_file, '-l'],
                    env=env,
                    capture_output=True,
                    text=True,
                    timeout=10
                )
                
                if result.returncode == 0:
                    print("✓ SC successfully used configuration file")
                    
                    # Check if agents from config directory were found
                    output = result.stdout
                    found_any = any(agent in output for agent in ['c_test_si', 'c_test_jbd', 'c_test_ac'])
                    
                    if found_any:
                        print("✓ Agents from configured directory discovered")
                    else:
                        print("✗ No agents found from configured directory")
                
                else:
                    print(f"✗ SC failed with configuration file: {result.stderr}")
            
            except Exception as e:
                print(f"✗ Error testing configuration: {e}")
        
        else:
            print("SKIP: SC binary not available for configuration test")
    
    def test_command_line_options(self):
        """Test various command-line options"""
        print("\n" + "="*60)
        print("Testing Command Line Options")
        print("="*60)
        
        if not self.sc_binary:
            print("SKIP: SC binary not available")
            return
        
        env = os.environ.copy()
        env['SOLARD_BINDIR'] = self.agent_dir
        
        # Test help option
        try:
            result = subprocess.run(
                [self.sc_binary, '-h'],
                capture_output=True,
                text=True,
                timeout=5
            )
            
            if result.returncode == 0 and 'Usage:' in result.stdout:
                print("✓ Help option (-h) works")
            else:
                print("✗ Help option (-h) failed")
        except Exception as e:
            print(f"✗ Help option error: {e}")
        
        # Test version/info (if available)
        try:
            result = subprocess.run(
                [self.sc_binary],  # No arguments should show help
                capture_output=True,
                text=True,
                timeout=5
            )
            
            if 'Service Controller' in result.stdout:
                print("✓ Default help message displays")
            else:
                print("✗ Default help message not shown")
        except Exception as e:
            print(f"✗ Default help error: {e}")
        
        # Test debug option
        try:
            result = subprocess.run(
                [self.sc_binary, '-d', '-l'],
                env=env,
                capture_output=True,
                text=True,
                timeout=10
            )
            
            # Debug mode should still work, just with more output
            if result.returncode == 0:
                print("✓ Debug option (-d) works")
            else:
                print("✗ Debug option (-d) failed")
        except Exception as e:
            print(f"✗ Debug option error: {e}")
    
    def cleanup(self):
        """Clean up test environment"""
        shutil.rmtree(self.test_dir, ignore_errors=True)
        print(f"Test environment cleaned up: {self.test_dir}")


def run_c_backend_tests():
    """Run complete C backend test suite"""
    print("SERVICE CONTROLLER C BACKEND TESTING")
    print("=" * 60)
    print(f"Test Date: {time.strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"Working Directory: {os.getcwd()}")
    print()
    
    tester = CBackendTester()
    
    try:
        # Run all tests
        tester.test_solard_bindir_functionality()
        tester.test_agent_discovery()
        tester.test_json_export()
        tester.test_web_export_mode()
        tester.test_managed_agents_functionality()
        tester.test_configuration_integration()
        tester.test_command_line_options()
        
        print("\n" + "=" * 60)
        print("C BACKEND TESTING COMPLETE")
        print("=" * 60)
        
        if tester.sc_binary:
            print("✓ All tests executed with live SC binary")
        else:
            print("⚠️  Some tests skipped - SC binary not available")
            print("   Build SC with 'make' to run complete tests")
        
        print(f"\nTest artifacts available in: {tester.test_dir}")
        print("Review test output above for specific results")
    
    finally:
        # Cleanup
        tester.cleanup()


if __name__ == '__main__':
    run_c_backend_tests()