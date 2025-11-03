#!/bin/bash

# Service Controller Web Interface Startup Script

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
VENV_DIR="$SCRIPT_DIR/venv"
PYTHON_EXEC="python3"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if Python 3 is available
check_python() {
    if ! command -v $PYTHON_EXEC &> /dev/null; then
        log_error "Python 3 is not installed or not in PATH"
        exit 1
    fi
    
    log_info "Using Python: $(which $PYTHON_EXEC) ($(python3 --version))"
}

# Create virtual environment if it doesn't exist
setup_venv() {
    if [ ! -d "$VENV_DIR" ]; then
        log_info "Creating virtual environment..."
        $PYTHON_EXEC -m venv "$VENV_DIR"
    fi
    
    log_info "Activating virtual environment..."
    source "$VENV_DIR/bin/activate"
    
    # Upgrade pip
    pip install --upgrade pip > /dev/null 2>&1
    
    # Install dependencies
    log_info "Installing Python dependencies..."
    pip install -r "$SCRIPT_DIR/requirements.txt"
    
    log_success "Virtual environment ready"
}

# Check if SC binary exists
check_sc_binary() {
    if [ ! -f "$PROJECT_ROOT/sc" ]; then
        log_warning "SC binary not found at $PROJECT_ROOT/sc"
        log_info "Building SC binary..."
        
        cd "$PROJECT_ROOT"
        if make clean && make; then
            log_success "SC binary built successfully"
        else
            log_error "Failed to build SC binary"
            exit 1
        fi
    else
        log_success "SC binary found at $PROJECT_ROOT/sc"
    fi
}

# Export environment variables
setup_environment() {
    export SC_BINARY_PATH="$PROJECT_ROOT/sc"
    export SC_DATA_FILE="/tmp/sc_data.json"
    export AGENT_DIR="/opt/sd/bin"
    export FLASK_ENV="development"
    export FLASK_DEBUG="true"
    
    log_info "Environment variables set:"
    log_info "  SC_BINARY_PATH=$SC_BINARY_PATH"
    log_info "  SC_DATA_FILE=$SC_DATA_FILE"
    log_info "  AGENT_DIR=$AGENT_DIR"
}

# Start the Flask application
start_flask() {
    log_info "Starting Flask web interface..."
    log_info "Dashboard will be available at: http://localhost:5001"
    log_info ""
    log_info "Press Ctrl+C to stop the server"
    log_info ""
    
    cd "$SCRIPT_DIR"
    source "$VENV_DIR/bin/activate"
    python app.py
}

# Cleanup function
cleanup() {
    log_info ""
    log_info "Shutting down..."
    
    # Kill any background SC processes
    pkill -f "sc -w" 2>/dev/null || true
    
    log_success "Cleanup complete"
}

# Set up signal handlers
trap cleanup EXIT INT TERM

# Main execution
main() {
    log_info "Starting Service Controller Web Interface"
    log_info "============================================"
    
    check_python
    setup_venv
    check_sc_binary
    setup_environment
    start_flask
}

# Parse command line arguments
case "${1:-}" in
    --help|-h)
        echo "Service Controller Web Interface Startup Script"
        echo ""
        echo "Usage: $0 [options]"
        echo ""
        echo "Options:"
        echo "  --help, -h     Show this help message"
        echo "  --clean        Clean virtual environment and reinstall dependencies"
        echo ""
        echo "Environment Variables:"
        echo "  SC_BINARY_PATH   Path to SC binary (default: ../sc)"
        echo "  SC_DATA_FILE     Path to SC data file (default: /tmp/sc_data.json)"
        echo "  AGENT_DIR        Directory containing agents (default: /opt/sd/bin)"
        echo "  PORT             Flask server port (default: 5001)"
        echo ""
        exit 0
        ;;
    --clean)
        log_info "Cleaning virtual environment..."
        rm -rf "$VENV_DIR"
        log_success "Virtual environment cleaned"
        ;;
esac

# Run main function
main "$@"