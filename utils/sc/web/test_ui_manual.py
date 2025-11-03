#!/usr/bin/env python3
"""
Manual UI Testing Script for Service Controller Agent Management
================================================================

This script provides manual testing procedures for the web interface
functionality that cannot be easily automated.

Author: TESTER Agent
Date: 2025-07-13
"""

import os
import sys
import json
import time
import subprocess
import tempfile
import webbrowser
from pathlib import Path

class UITestManager:
    """Manager for UI testing procedures"""
    
    def __init__(self):
        self.test_dir = tempfile.mkdtemp(prefix='sc_ui_test_')
        self.agent_dir = os.path.join(self.test_dir, 'agents')
        self.setup_test_environment()
    
    def setup_test_environment(self):
        """Set up test environment for UI testing"""
        print("Setting up UI test environment...")
        
        # Create agent directory
        os.makedirs(self.agent_dir, exist_ok=True)
        
        # Create fake agents
        self.create_test_agents()
        
        # Set environment variables
        os.environ['SOLARD_BINDIR'] = self.agent_dir
        os.environ['SC_DATA_FILE'] = os.path.join(self.test_dir, 'sc_data.json')
        os.environ['SC_BINARY_PATH'] = self.create_fake_sc_binary()
        
        print(f"Test environment ready: {self.test_dir}")
        print(f"Agent directory: {self.agent_dir}")
    
    def create_test_agents(self):
        """Create test agents for UI testing"""
        agents = [
            {'name': 'ui_test_si', 'role': 'inverter', 'version': '1.0.0'},
            {'name': 'ui_test_jbd', 'role': 'battery', 'version': '1.2.0'},
            {'name': 'ui_test_ac', 'role': 'controller', 'version': '2.1.0'},
            {'name': 'ui_test_sensor', 'role': 'sensor', 'version': '0.5.0'},
        ]
        
        for agent in agents:
            agent_path = os.path.join(self.agent_dir, agent['name'])
            script_content = f'''#!/bin/bash
if [ "$1" = "-I" ]; then
    echo '{{"agent_name": "{agent['name']}", "agent_role": "{agent['role']}", "agent_version": "{agent['version']}"}}'
    exit 0
else
    echo "Test agent {agent['name']}"
    exit 0
fi
'''
            with open(agent_path, 'w') as f:
                f.write(script_content)
            os.chmod(agent_path, 0o755)
        
        print(f"Created {len(agents)} test agents")
    
    def create_fake_sc_binary(self):
        """Create a fake SC binary for testing"""
        sc_path = os.path.join(self.test_dir, 'fake_sc')
        sc_content = '''#!/bin/bash
# Fake SC binary for UI testing
echo "Fake SC binary called with: $@"
if [ "$1" = "-w" ]; then
    echo "Web export mode started..."
    while true; do
        sleep 5
        echo "SC running..."
    done
fi
'''
        with open(sc_path, 'w') as f:
            f.write(sc_content)
        os.chmod(sc_path, 0o755)
        return sc_path
    
    def start_flask_server(self):
        """Start the Flask development server"""
        print("Starting Flask development server...")
        
        # Change to web directory
        web_dir = os.path.dirname(os.path.abspath(__file__))
        os.chdir(web_dir)
        
        # Start server
        env = os.environ.copy()
        env['FLASK_DEBUG'] = 'True'
        env['PORT'] = '5001'  # Use different port for testing
        
        try:
            process = subprocess.Popen(
                [sys.executable, 'app.py'],
                env=env,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )
            
            # Wait a moment for server to start
            time.sleep(3)
            
            print("Flask server started on http://localhost:5001")
            return process
            
        except Exception as e:
            print(f"Failed to start Flask server: {e}")
            return None
    
    def print_test_procedures(self):
        """Print manual testing procedures"""
        print("\n" + "=" * 80)
        print("MANUAL UI TESTING PROCEDURES")
        print("=" * 80)
        
        procedures = [
            {
                'title': 'Dashboard Display Test',
                'steps': [
                    '1. Open http://localhost:5001 in your browser',
                    '2. Verify the dashboard loads correctly',
                    '3. Check that agent counts and status are displayed',
                    '4. Verify responsive design on different screen sizes'
                ]
            },
            {
                'title': 'Add Agent Modal Test',
                'steps': [
                    '1. Click the "Add Agent" button',
                    '2. Verify modal opens with two tabs: "From Directory" and "Custom Path"',
                    '3. Test "From Directory" tab:',
                    '   - Verify list of available agents appears',
                    '   - Select an agent and click "Add Selected Agent"',
                    '   - Verify success message and modal closes',
                    '4. Test "Custom Path" tab:',
                    '   - Enter a valid agent path',
                    '   - Click "Add Agent"',
                    '   - Verify validation and success handling'
                ]
            },
            {
                'title': 'Agent List Management Test',
                'steps': [
                    '1. Verify agents appear in the main list after adding',
                    '2. Test agent removal:',
                    '   - Click "Remove" button for an agent',
                    '   - Verify confirmation dialog',
                    '   - Confirm removal and verify agent disappears',
                    '3. Test agent status updates in real-time'
                ]
            },
            {
                'title': 'Error Handling Test',
                'steps': [
                    '1. Try adding invalid agent path',
                    '2. Try adding duplicate agent',
                    '3. Try removing non-existent agent',
                    '4. Verify error messages are clear and helpful'
                ]
            },
            {
                'title': 'Real-time Updates Test',
                'steps': [
                    '1. Keep dashboard open',
                    '2. Add/remove agents via API calls',
                    '3. Verify UI updates automatically',
                    '4. Check WebSocket/polling functionality'
                ]
            },
            {
                'title': 'Browser Compatibility Test',
                'steps': [
                    '1. Test in Chrome',
                    '2. Test in Firefox',
                    '3. Test in Safari (if on macOS)',
                    '4. Test in Edge',
                    '5. Verify all functionality works across browsers'
                ]
            },
            {
                'title': 'Mobile Responsiveness Test',
                'steps': [
                    '1. Open browser developer tools',
                    '2. Toggle device simulation',
                    '3. Test various screen sizes (phone, tablet)',
                    '4. Verify layout adapts correctly',
                    '5. Test touch interactions'
                ]
            }
        ]
        
        for i, procedure in enumerate(procedures, 1):
            print(f"\n{i}. {procedure['title']}")
            print("-" * (len(procedure['title']) + 3))
            for step in procedure['steps']:
                print(f"   {step}")
    
    def generate_test_data(self):
        """Generate test API calls for manual testing"""
        print("\n" + "=" * 80)
        print("API TEST COMMANDS")
        print("=" * 80)
        
        commands = [
            {
                'description': 'Get available agents',
                'command': 'curl -X GET http://localhost:5001/api/available-agents'
            },
            {
                'description': 'Add an agent',
                'command': '''curl -X POST http://localhost:5001/api/agents/add \\
  -H "Content-Type: application/json" \\
  -d '{"name": "ui_test_jbd", "path": "''' + os.path.join(self.agent_dir, 'ui_test_jbd') + '''"}'
            },
            {
                'description': 'Get all agents',
                'command': 'curl -X GET http://localhost:5001/api/agents'
            },
            {
                'description': 'Remove an agent',
                'command': 'curl -X DELETE http://localhost:5001/api/agents/ui_test_jbd/remove'
            },
            {
                'description': 'Get system status',
                'command': 'curl -X GET http://localhost:5001/api/system/status'
            }
        ]
        
        for i, cmd in enumerate(commands, 1):
            print(f"\n{i}. {cmd['description']}:")
            print(f"   {cmd['command']}")
    
    def create_test_checklist(self):
        """Create a test checklist file"""
        checklist_file = os.path.join(self.test_dir, 'ui_test_checklist.md')
        
        checklist_content = """# Service Controller UI Test Checklist

## Environment Setup
- [ ] Test environment created successfully
- [ ] Fake agents created and executable
- [ ] Flask server started on port 5001
- [ ] Test data populated

## Dashboard Tests
- [ ] Dashboard loads without errors
- [ ] Agent count displays correctly
- [ ] System status shows properly
- [ ] Layout is responsive
- [ ] Navigation works

## Add Agent Modal Tests
- [ ] Modal opens when "Add Agent" clicked
- [ ] "From Directory" tab works
- [ ] Available agents list populates
- [ ] Agent selection works
- [ ] "Custom Path" tab works
- [ ] Path validation works
- [ ] Success messages display
- [ ] Modal closes after success
- [ ] Error handling works

## Agent Management Tests
- [ ] Added agents appear in list
- [ ] Agent details display correctly
- [ ] Remove button works
- [ ] Confirmation dialog appears
- [ ] Agent removal completes
- [ ] List updates after removal

## Real-time Updates
- [ ] Dashboard updates automatically
- [ ] Agent status changes reflect
- [ ] New agents appear without refresh
- [ ] Removed agents disappear

## Error Handling
- [ ] Invalid path errors display
- [ ] Duplicate agent errors show
- [ ] Network errors handled gracefully
- [ ] Loading states work

## Browser Compatibility
- [ ] Chrome works correctly
- [ ] Firefox works correctly
- [ ] Safari works correctly (macOS)
- [ ] Edge works correctly

## Mobile Responsiveness
- [ ] Phone layout works
- [ ] Tablet layout works
- [ ] Touch interactions work
- [ ] Modals display properly on mobile

## Performance
- [ ] Page loads quickly
- [ ] API calls respond promptly
- [ ] No memory leaks observed
- [ ] Smooth animations

## Accessibility
- [ ] Keyboard navigation works
- [ ] Screen reader compatibility
- [ ] High contrast mode support
- [ ] Tab order is logical

## Notes
Add any issues or observations here:
"""
        
        with open(checklist_file, 'w') as f:
            f.write(checklist_content)
        
        print(f"\nTest checklist created: {checklist_file}")
        return checklist_file
    
    def cleanup(self):
        """Clean up test environment"""
        import shutil
        shutil.rmtree(self.test_dir, ignore_errors=True)
        print(f"Test environment cleaned up: {self.test_dir}")


def main():
    """Main function for manual UI testing"""
    print("SERVICE CONTROLLER UI MANUAL TESTING")
    print("=" * 50)
    
    # Create test manager
    test_manager = UITestManager()
    
    try:
        # Start Flask server
        flask_process = test_manager.start_flask_server()
        
        if not flask_process:
            print("Failed to start Flask server. Exiting.")
            return
        
        # Print test procedures
        test_manager.print_test_procedures()
        
        # Generate API test commands
        test_manager.generate_test_data()
        
        # Create test checklist
        checklist_file = test_manager.create_test_checklist()
        
        print("\n" + "=" * 80)
        print("MANUAL TESTING SETUP COMPLETE")
        print("=" * 80)
        print(f"Flask server: http://localhost:5001")
        print(f"Test checklist: {checklist_file}")
        print(f"Test environment: {test_manager.test_dir}")
        
        # Optionally open browser
        try:
            print("\nOpening browser...")
            webbrowser.open('http://localhost:5001')
        except:
            print("Could not open browser automatically. Please open http://localhost:5001 manually.")
        
        print("\nPress Ctrl+C when testing is complete...")
        
        # Keep server running
        try:
            flask_process.wait()
        except KeyboardInterrupt:
            print("\nShutting down...")
            flask_process.terminate()
            flask_process.wait()
        
    finally:
        # Cleanup
        test_manager.cleanup()


if __name__ == '__main__':
    main()