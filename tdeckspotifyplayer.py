import os
import serial
import time
import spotipy
from spotipy.oauth2 import SpotifyOAuth
from datetime import datetime

# Load credentials from environment variables
SPOTIPY_CLIENT_ID = os.getenv("CLIENT_ID")
SPOTIPY_CLIENT_SECRET = os.getenv("CLIENT_SECRET")
SPOTIPY_REDIRECT_URI = "http://localhost:8227/callback"

# Spotify Authentication with expanded scope
scope = "user-library-read user-library-modify user-read-currently-playing user-read-playback-state user-modify-playback-state"
sp = spotipy.Spotify(auth_manager=SpotifyOAuth(
    client_id=SPOTIPY_CLIENT_ID,
    client_secret=SPOTIPY_CLIENT_SECRET,
    redirect_uri=SPOTIPY_REDIRECT_URI,
    scope=scope
))

# Serial Port Configuration
SERIAL_PORT = "COM8"  # Change for Linux/macOS (e.g., "/dev/ttyUSB0")
BAUD_RATE = 115200  # Match firmware baud rate


def ms_to_time_str(ms):
    """Convert milliseconds to MM:SS format"""
    seconds = int(ms / 1000)
    minutes = seconds // 60
    seconds = seconds % 60
    return f"{minutes}:{seconds:02d}"


def get_current_track_info():
    """Fetch detailed info about the currently playing track"""
    try:
        current = sp.currently_playing()
        if current and current["item"]:
            track = current["item"]

            # Get playback state including shuffle
            playback_state = sp.current_playback()
            shuffle_state = playback_state.get('shuffle_state', False) if playback_state else False

            return {
                "id": track["id"],
                "name": track["name"],
                "artist": track["artists"][0]["name"],
                "is_playing": current["is_playing"],
                "progress_ms": current["progress_ms"],
                "duration_ms": track["duration_ms"],
                "progress_percent": int((current["progress_ms"] / track["duration_ms"]) * 100),
                "shuffle": shuffle_state
            }
    except Exception as e:
        print(f"Error getting track info: {e}")
        return None
    return None


def update_display(ser, track_info):
    """Send track information to the display"""
    try:
        if track_info:
            # Send track name
            ser.write(f"TRACK:Now Playing: {track_info['name']}\n".encode())
            # Send artist name
            ser.write(f"ARTIST:Artist: {track_info['artist']}\n".encode())
            # Send progress
            ser.write(f"PROGRESS:{track_info['progress_percent']}\n".encode())
            # Send time
            time_str = f"{ms_to_time_str(track_info['progress_ms'])} / {ms_to_time_str(track_info['duration_ms'])}"
            ser.write(f"TIME:{time_str}\n".encode())
    except Exception as e:
        print(f"Error updating display: {e}")


def handle_command(cmd):
    """Handle commands received from the T-Deck"""
    try:
        if cmd.startswith("CMD:"):
            action = cmd[4:].strip()
            if action == "PLAY":
                current = sp.currently_playing()
                if current and current["is_playing"]:
                    sp.pause_playback()
                else:
                    sp.start_playback()
            elif action == "NEXT":
                sp.next_track()
            elif action == "PREV":
                sp.previous_track()
            elif action == "SHUFFLE":
                playback_state = sp.current_playback()
                if playback_state:
                    current_shuffle = playback_state.get('shuffle_state', False)
                    sp.shuffle(not current_shuffle)
            elif action == "LIKE":
                track_info = get_current_track_info()
                if track_info:
                    sp.current_user_saved_tracks_add([track_info["id"]])
                    print("✅ Song liked!")

        elif cmd.startswith("SEEK:"):
            position = int(cmd[5:].strip())
            track_info = get_current_track_info()
            if track_info:
                position_ms = int((position / 100) * track_info["duration_ms"])
                sp.seek_track(position_ms)

        elif cmd.startswith("KEY:"):
            key = cmd[4:].strip()
            print(f"Keyboard key pressed: {key}")
            # Add custom keyboard shortcuts here if needed

    except Exception as e:
        print(f"Error handling command {cmd}: {e}")


def main():
    last_update = 0
    update_interval = 1.0  # Update display every second

    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"Listening on {SERIAL_PORT} at {BAUD_RATE} baud...")

        buffer = ""
        while True:
            # Handle incoming serial data
            if ser.in_waiting:
                char = ser.read().decode("utf-8", errors="ignore")
                if char == "\n" or char == "\r":
                    if buffer:
                        print(f"Received command: {buffer}")
                        handle_command(buffer)
                    buffer = ""
                else:
                    buffer += char

            # Update display periodically
            current_time = time.time()
            if current_time - last_update >= update_interval:
                track_info = get_current_track_info()
                if track_info:
                    update_display(ser, track_info)
                last_update = current_time
            # Small delay to prevent excessive CPU usage
            time.sleep(0.01)

    except serial.SerialException as e:
        print(f"Serial error: {e}")
    except KeyboardInterrupt:
        print("\nExiting...")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()


if __name__ == "__main__":
    main()