#!/bin/sh
# Script App - Example app with no compiled binary
# This script is executed by the NAK's loader, not directly.
#
# The NAK loader (framework_loader) receives this script path
# via the args_template and can interpret/execute it.

echo "Script App v1.0.0"
echo "================="
echo ""
echo "This app has NO compiled binary!"
echo "It's a shell script executed via the NAK loader."
echo ""
echo "Environment from NAH:"
echo "  NAH_APP_ID=$NAH_APP_ID"
echo "  NAH_APP_VERSION=$NAH_APP_VERSION"
echo "  NAH_APP_ROOT=$NAH_APP_ROOT"
echo "  NAH_NAK_ID=$NAH_NAK_ID"
echo "  NAH_NAK_ROOT=$NAH_NAK_ROOT"
echo ""

# Read config if it exists
if [ -f "$NAH_APP_ROOT/assets/config.txt" ]; then
    echo "Config file contents:"
    cat "$NAH_APP_ROOT/assets/config.txt"
    echo ""
fi

echo "Script completed successfully!"
