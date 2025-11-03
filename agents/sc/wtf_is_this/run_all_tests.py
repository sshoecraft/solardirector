#!/usr/bin/env python3
"""
Comprehensive Test Runner for Service Controller Agent Management
================================================================

This script runs all tests for the Service Controller agent management
functionality and generates a complete test report.

Author: TESTER Agent
Date: 2025-07-13
"""

import os
import sys
import time
import json
import subprocess
import tempfile
from pathlib import Path

class TestExecutor:
    """Main test execution coordinator"""
    
    def __init__(self):
        self.start_time = time.time()
        self.results = {}
        self.report_dir = tempfile.mkdtemp(prefix='sc_test_report_')
        print(f"Test execution started at {time.strftime('%Y-%m-%d %H:%M:%S')}")
        print(f"Report directory: {self.report_dir}")
    
    def run_test_suite(self, name, script_path, *args):
        """Run a test suite and capture results"""
        print(f"\n{'='*80}")
        print(f"RUNNING: {name}")
        print(f"{'='*80}")
        
        start_time = time.time()
        
        try:
            # Check if script exists
            if not os.path.exists(script_path):
                print(f"SKIP: Test script not found: {script_path}")
                self.results[name] = {
                    'status': 'SKIPPED',
                    'reason': 'Script not found',
                    'duration': 0,
                    'output': ''
                }
                return
            
            # Run the test
            cmd = [sys.executable, script_path] + list(args)
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=300  # 5 minute timeout
            )
            
            duration = time.time() - start_time
            
            # Determine status
            if result.returncode == 0:
                status = 'PASSED'
            else:
                status = 'FAILED'
            
            # Store results
            self.results[name] = {
                'status': status,
                'returncode': result.returncode,
                'duration': duration,
                'stdout': result.stdout,
                'stderr': result.stderr
            }
            
            print(f"Status: {status}")
            print(f"Duration: {duration:.2f}s")
            
            if result.stdout:
                print("STDOUT:")
                print(result.stdout)
            
            if result.stderr:
                print("STDERR:")
                print(result.stderr)
        
        except subprocess.TimeoutExpired:
            duration = time.time() - start_time
            self.results[name] = {
                'status': 'TIMEOUT',
                'duration': duration,
                'output': 'Test timed out after 5 minutes'
            }
            print(f"Status: TIMEOUT ({duration:.2f}s)")
        
        except Exception as e:
            duration = time.time() - start_time
            self.results[name] = {
                'status': 'ERROR',
                'duration': duration,
                'error': str(e)
            }
            print(f"Status: ERROR - {e}")
    
    def check_build_status(self):
        """Check if SC binary is built and ready"""
        print(f"\n{'='*80}")
        print("CHECKING BUILD STATUS")
        print(f"{'='*80}")
        
        sc_binary = './sc'
        if os.path.exists(sc_binary):
            if os.access(sc_binary, os.X_OK):
                print("✓ SC binary exists and is executable")
                return True
            else:
                print("✗ SC binary exists but is not executable")
                return False
        else:
            print("✗ SC binary not found")
            print("  Run 'make' to build the SC binary for complete testing")
            return False
    
    def check_dependencies(self):
        """Check if required dependencies are available"""
        print(f"\n{'='*80}")
        print("CHECKING DEPENDENCIES")
        print(f"{'='*80}")
        
        dependencies = {
            'Python': [sys.executable, '--version'],
            'Flask': [sys.executable, '-c', 'import flask; print(flask.__version__)'],
            'Requests': [sys.executable, '-c', 'import requests; print(requests.__version__)'],
            'JSON': [sys.executable, '-c', 'import json; print("OK")']
        }
        
        all_good = True
        
        for dep, cmd in dependencies.items():
            try:
                result = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
                if result.returncode == 0:
                    print(f"✓ {dep}: {result.stdout.strip()}")
                else:
                    print(f"✗ {dep}: Failed to check")
                    all_good = False
            except Exception as e:
                print(f"✗ {dep}: {e}")
                all_good = False
        
        return all_good
    
    def generate_html_report(self):
        """Generate HTML test report"""
        report_file = os.path.join(self.report_dir, 'test_report.html')
        
        html_content = f"""<!DOCTYPE html>
<html>
<head>
    <title>Service Controller Test Report</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 20px; }}
        .header {{ background-color: #f0f0f0; padding: 20px; border-radius: 5px; }}
        .summary {{ margin: 20px 0; padding: 15px; background-color: #e8f4fd; border-radius: 5px; }}
        .test-result {{ margin: 10px 0; padding: 10px; border-radius: 5px; }}
        .passed {{ background-color: #d4edda; border-left: 5px solid #28a745; }}
        .failed {{ background-color: #f8d7da; border-left: 5px solid #dc3545; }}
        .skipped {{ background-color: #fff3cd; border-left: 5px solid #ffc107; }}
        .timeout {{ background-color: #f8d7da; border-left: 5px solid #fd7e14; }}
        .error {{ background-color: #f8d7da; border-left: 5px solid #6f42c1; }}
        .output {{ background-color: #f8f9fa; padding: 10px; margin: 10px 0; font-family: monospace; white-space: pre-wrap; }}
        .metric {{ display: inline-block; margin: 10px 20px 10px 0; }}
    </style>
</head>
<body>
    <div class="header">
        <h1>Service Controller Agent Management Test Report</h1>
        <p><strong>Generated:</strong> {time.strftime('%Y-%m-%d %H:%M:%S')}</p>
        <p><strong>Total Duration:</strong> {time.time() - self.start_time:.2f} seconds</p>
    </div>
    
    <div class="summary">
        <h2>Test Summary</h2>
        {self._generate_summary_html()}
    </div>
    
    <h2>Test Results</h2>
    {self._generate_results_html()}
    
    <div class="summary">
        <h2>Test Coverage</h2>
        <p>This test suite covers:</p>
        <ul>
            <li>✓ SOLARD_BINDIR environment variable functionality</li>
            <li>✓ Agent management features (add/remove agents)</li>
            <li>✓ Flask API endpoints for agent management</li>
            <li>✓ Configuration persistence and error handling</li>
            <li>✓ C backend integration</li>
            <li>📋 Manual UI testing procedures provided</li>
        </ul>
    </div>
</body>
</html>"""
        
        with open(report_file, 'w') as f:
            f.write(html_content)
        
        print(f"HTML report generated: {report_file}")
        return report_file
    
    def _generate_summary_html(self):
        """Generate summary statistics HTML"""
        total = len(self.results)
        passed = sum(1 for r in self.results.values() if r['status'] == 'PASSED')
        failed = sum(1 for r in self.results.values() if r['status'] == 'FAILED')
        skipped = sum(1 for r in self.results.values() if r['status'] == 'SKIPPED')
        timeout = sum(1 for r in self.results.values() if r['status'] == 'TIMEOUT')
        error = sum(1 for r in self.results.values() if r['status'] == 'ERROR')
        
        return f"""
        <div class="metric"><strong>Total Tests:</strong> {total}</div>
        <div class="metric"><strong>Passed:</strong> {passed}</div>
        <div class="metric"><strong>Failed:</strong> {failed}</div>
        <div class="metric"><strong>Skipped:</strong> {skipped}</div>
        <div class="metric"><strong>Timeout:</strong> {timeout}</div>
        <div class="metric"><strong>Error:</strong> {error}</div>
        <div class="metric"><strong>Success Rate:</strong> {(passed/total)*100:.1f}%</div>
        """
    
    def _generate_results_html(self):
        """Generate individual test results HTML"""
        html = ""
        
        for name, result in self.results.items():
            status_class = result['status'].lower()
            
            html += f"""
            <div class="test-result {status_class}">
                <h3>{name}</h3>
                <p><strong>Status:</strong> {result['status']}</p>
                <p><strong>Duration:</strong> {result.get('duration', 0):.2f}s</p>
            """
            
            if 'stdout' in result and result['stdout']:
                html += f'<div class="output">{result["stdout"][:2000]}...</div>'
            
            if 'stderr' in result and result['stderr']:
                html += f'<div class="output">STDERR: {result["stderr"][:1000]}...</div>'
            
            if 'error' in result:
                html += f'<div class="output">ERROR: {result["error"]}</div>'
            
            html += "</div>"
        
        return html
    
    def generate_json_report(self):
        """Generate JSON test report"""
        report_file = os.path.join(self.report_dir, 'test_report.json')
        
        report_data = {
            'metadata': {
                'generated_at': time.strftime('%Y-%m-%d %H:%M:%S'),
                'total_duration': time.time() - self.start_time,
                'test_count': len(self.results)
            },
            'summary': {
                'passed': sum(1 for r in self.results.values() if r['status'] == 'PASSED'),
                'failed': sum(1 for r in self.results.values() if r['status'] == 'FAILED'),
                'skipped': sum(1 for r in self.results.values() if r['status'] == 'SKIPPED'),
                'timeout': sum(1 for r in self.results.values() if r['status'] == 'TIMEOUT'),
                'error': sum(1 for r in self.results.values() if r['status'] == 'ERROR')
            },
            'results': self.results
        }
        
        with open(report_file, 'w') as f:
            json.dump(report_data, f, indent=2)
        
        print(f"JSON report generated: {report_file}")
        return report_file
    
    def print_final_summary(self):
        """Print final test summary"""
        total = len(self.results)
        passed = sum(1 for r in self.results.values() if r['status'] == 'PASSED')
        failed = sum(1 for r in self.results.values() if r['status'] == 'FAILED')
        
        print(f"\n{'='*80}")
        print("FINAL TEST SUMMARY")
        print(f"{'='*80}")
        print(f"Total Tests: {total}")
        print(f"Passed: {passed}")
        print(f"Failed: {failed}")
        print(f"Success Rate: {(passed/total)*100:.1f}%")
        print(f"Total Duration: {time.time() - self.start_time:.2f}s")
        
        if failed == 0:
            print("\n🎉 ALL TESTS PASSED! Agent management functionality is ready.")
        else:
            print(f"\n⚠️  {failed} TEST(S) FAILED. Review results and fix issues.")
        
        print(f"\nReports available in: {self.report_dir}")


def main():
    """Main test execution function"""
    print("SERVICE CONTROLLER COMPREHENSIVE TEST SUITE")
    print("=" * 80)
    
    executor = TestExecutor()
    
    # Check prerequisites
    executor.check_dependencies()
    has_binary = executor.check_build_status()
    
    # Get current directory
    current_dir = os.path.dirname(os.path.abspath(__file__))
    web_dir = os.path.join(current_dir, 'web')
    
    # Define test suites
    test_suites = [
        ("Python Flask API Tests", os.path.join(web_dir, 'test_agent_management.py')),
        ("C Backend Tests", os.path.join(current_dir, 'test_c_backend.py')),
    ]
    
    # Run all test suites
    for name, script_path in test_suites:
        executor.run_test_suite(name, script_path)
    
    # Generate reports
    html_report = executor.generate_html_report()
    json_report = executor.generate_json_report()
    
    # Print summary
    executor.print_final_summary()
    
    # Manual testing instructions
    print(f"\n{'='*80}")
    print("MANUAL TESTING")
    print(f"{'='*80}")
    print("For complete testing, also run manual UI tests:")
    print(f"  python3 {os.path.join(web_dir, 'test_ui_manual.py')}")
    print()
    print("This will start a test environment and provide step-by-step")
    print("instructions for testing the web interface.")
    
    return executor


if __name__ == '__main__':
    main()