#!/bin/bash

set -e

SIS_PATH="$HOME/.config/sis/"
LINK="https://api.github.com/repos/Julius-IX/SiS/releases"
ZIP_NAME="sis-linux-x86_64.tar.gz"

mkdir -p "$SIS_PATH"

TMP_FILE="/tmp/$ZIP_NAME"

echo "Fetching latest release..."

DOWNLOAD_URL=$(curl -s "$LINK" \
  | grep "browser_download_url" \
  | grep "$ZIP_NAME" \
  | head -n 1 \
  | cut -d '"' -f 4)

if [ -z "$DOWNLOAD_URL" ]; then
    echo "Failed to find download URL."
    exit 1
fi

echo "Downloading..."
curl -L "$DOWNLOAD_URL" -o "$TMP_FILE"

echo "Extracting..."
tar -xzf "$TMP_FILE" -C "$SIS_PATH"

rm -f "$TMP_FILE"

CURRENT_SHELL=$(basename "${SHELL:-}")

add_if_missing() {
    local line="$1"
    local file="$2"

    mkdir -p "$(dirname "$file")"
    touch "$file"

    grep -Fxq "$line" "$file" || echo "$line" >> "$file"
}

case "$CURRENT_SHELL" in
    bash)
        CONFIG="$HOME/.bashrc"

        add_if_missing "export SIS_PATH=\"$HOME/.config/sis\"" "$CONFIG"
        add_if_missing "export PATH=\"$SIS_PATH:$PATH\"" "$CONFIG"

        echo "Updated $CONFIG"
        echo "Run: source $CONFIG"
        ;;

    zsh)
        CONFIG="$HOME/.zshrc"

        add_if_missing "export SIS_PATH=\"$HOME/.config/sis\"" "$CONFIG"
        add_if_missing "export PATH=\"$SIS_PATH:$PATH\"" "$CONFIG"

        echo "Updated $CONFIG"
        echo "Run: source $CONFIG"
        ;;

    fish)
        CONFIG="$HOME/.config/fish/config.fish"

        add_if_missing "set -gx SIS_PATH ~/.config/sis" "$CONFIG"
        add_if_missing "fish_add_path $SIS_PATH" "$CONFIG"

        echo "Updated $CONFIG"
        echo "Run: source $CONFIG"
        ;;

    *)
        echo
        echo "Installed to $SIS_PATH"
        echo "Could not automatically configure shell '$CURRENT_SHELL'."
        echo
        echo "Add the following manually:"
        echo "export SIS_PATH=\"$HOME/.config/sis\""
        echo "export PATH=\"$SIS_PATH:$PATH\""
        ;;
esac

echo
echo "Installed to $SIS_PATH"
