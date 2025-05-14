#!/bin/bash
# macOS script to run QEMU for Sebria OS with user-mode networking and debug

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "This script requires sudo to handle macOS permissions and networking."
    echo "Run: sudo ./run_sebria_os.sh"
    exit 1
fi

# Ensure QEMU is installed
if ! command -v qemu-system-x86_64 &> /dev/null; then
    echo "Error: qemu-system-x86_64 not found. Install QEMU (e.g., via Homebrew: brew install qemu)."
    exit 1
fi

# Check QEMU version
QEMU_VERSION=$(qemu-system-x86_64 --version | head -n1)
echo "QEMU Version: $QEMU_VERSION"
if [[ ! "$QEMU_VERSION" =~ "version 8" && ! "$QEMU_VERSION" =~ "version 9" ]]; then
    echo "Warning: QEMU version older than 8.x. Consider updating: brew upgrade qemu"
fi

# Check for os-image.iso
if [ ! -f "os-image.iso" ]; then
    echo "Error: os-image.iso not found in current directory."
    exit 1
fi

# Step 1: Check macOS firewall status
echo "Checking macOS firewall status..."
FIREWALL_STATE=$(/usr/libexec/ApplicationFirewall/socketfilterfw --getglobalstate 2>/dev/null)
if [[ "$FIREWALL_STATE" == *"enabled"* ]]; then
    echo "Warning: macOS firewall is enabled, which may block QEMU networking."
    echo "Disabling firewall temporarily..."
    /usr/libexec/ApplicationFirewall/socketfilterfw --setglobalstate off
fi

# Step 2: Check existing utun interfaces
echo "Existing utun interfaces before QEMU:"
ifconfig | grep -E '^utun[0-9]+' || echo "No utun interfaces found."

# Step 3: Try minimal QEMU configuration first
echo "Trying minimal QEMU configuration (no networking)..."
qemu-system-x86_64 \
    -m 2048 \
    -cdrom os-image.iso \
    -cpu host \
    -accel hvf \
    -d guest_errors,unimp \
    -D qemu_log_minimal.txt 2>&1 &

QEMU_PID=$!
sleep 5
if ! ps -p $QEMU_PID > /dev/null; then
    echo "Error: Minimal QEMU exited (PID: $QEMU_PID). Check qemu_log_minimal.txt."
    echo "Possible issues: Invalid os-image.iso or HVF permissions."
    echo "Trying with TCG acceleration..."
    qemu-system-x86_64 \
        -m 2048 \
        -cdrom os-image.iso \
        -cpu host \
        -accel tcg \
        -d guest_errors,unimp \
        -D qemu_log_minimal.txt 2>&1 &

    QEMU_PID=$!
    sleep 5
    if ! ps -p $QEMU_PID > /dev/null; then
        echo "Error: Minimal QEMU failed with TCG too. Check qemu_log_minimal.txt."
        echo "Verify os-image.iso and rebuild if needed: make clean && make"
        exit 1
    fi
fi
echo "Minimal QEMU running (PID: $QEMU_PID). Killing to try full config..."
kill $QEMU_PID
wait $QEMU_PID 2>/dev/null

# Step 4: Launch QEMU with full networking
echo "Starting QEMU with networking..."
qemu-system-x86_64 \
    -m 2048 \
    -cdrom os-image.iso \
    -cpu host \
    -netdev user,id=net0 \
    -device ne2k_isa,netdev=net0,mac=52:54:00:12:34:56,irq=9,iobase=0x300 \
    -accel hvf \
    -d guest_errors,unimp,net \
    -D qemu_log.txt 2>&1 &

QEMU_PID=$!
sleep 5
if ! ps -p $QEMU_PID > /dev/null; then
    echo "Error: QEMU with networking exited (PID: $QEMU_PID). Check qemu_log.txt."
    echo "Trying with TCG acceleration..."
    qemu-system-x86_64 \
        -m 2048 \
        -cdrom os-image.iso \
        -cpu host \
        -netdev user,id=net0 \
        -device ne2k_isa,netdev=net0,mac=52:54:00:12:34:56,irq=9,iobase=0x300 \
        -accel tcg \
        -d guest_errors,unimp,net \
        -D qemu_log.txt 2>&1 &

    QEMU_PID=$!
    sleep 5
    if ! ps -p $QEMU_PID > /dev/null; then
        echo "Error: QEMU with networking failed with TCG too. Check qemu_log.txt."
        exit 1
    fi
fi

# Step 5: Check for new utun interface
echo "utun interfaces after QEMU:"
NEW_UTUN=$(ifconfig | grep -E '^utun[0-9]+' || echo "No utun interfaces found.")
echo "$NEW_UTUN"
if [[ "$NEW_UTUN" == "$(ifconfig | grep -E '^utun[0-9]+' | sort)" ]]; then
    echo "Note: No new utun interface detected. User-mode networking may reuse an existing utun."
fi

# Step 6: Monitor traffic on utun interfaces
echo "Monitoring traffic with tcpdump..."
for i in {0..7}; do
    if ifconfig utun$i >/dev/null 2>&1; then
        echo "Starting tcpdump on utun$i..."
        tcpdump -i utun$i -n -c 10 'host 10.0.2.15 or host 10.0.2.2' 2>/dev/null &
    fi
done
echo "In another terminal, run: ping 10.0.2.15"
echo "Look for ARP or ICMP packets in tcpdump output."

# Step 7: Instructions for Sebria OS
echo "In Sebria OS:"
echo "1. Press 'S' to enter the shell."
echo "2. Type 'net' to initialize the network and display counters."
echo "   - Row 4: MAC: 52:54:00:12:34:56 (confirm correct MAC)."
echo "   - Row 6: Sending test ARP... (confirm ARP attempt)."
echo "   - Rows 2-3: Network: Connected, Rx: N, Tx: M (Tx >= 1 after ARP)."
echo "   - Row 5: RX Interrupt or TX Interrupt (should see TX after ARP)."
echo "   - Row 7: ISR: XX, RSR: XX, TSR: XX (TSR=01 for successful TX)."
echo "   - Row 8: IRQ 9 Fired (confirms interrupt delivery)."
echo "3. If Rx/Tx still 0, note TSR value (e.g., 08=buffer overflow) and check qemu_log.txt."
echo "4. Share output from rows 2-8, tcpdump results, and qemu_log.txt."

# Step 8: Keep script running until QEMU exits
echo "QEMU is running (PID: $QEMU_PID). Press Ctrl+C to stop."
wait $QEMU_PID
echo "QEMU has exited. Check qemu_log.txt for details."

# Cleanup
if [[ "$FIREWALL_STATE" == *"enabled"* ]]; then
    echo "Restoring firewall state..."
    /usr/libexec/ApplicationFirewall/socketfilterfw --setglobalstate on
fi
killall tcpdump 2>/dev/null