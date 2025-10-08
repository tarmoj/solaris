# Solaris Server - TTS Integration

## Overview

The Solaris Server is a WebSocket-based server that can generate Text-to-Speech (TTS) audio using the ElevenLabs API. It listens for special commands from WebSocket clients and generates MP3 audio files on demand.

## Architecture

### Components

1. **SolarisServer** (server/solarisserver.cpp) - The main WebSocket server
2. **generator.py** (audio/generator.py) - Python script that calls ElevenLabs API
3. **elevenlabs-api-key.sh** (audio/elevenlabs-api-key.sh) - Shell script containing API key

### How It Works

1. Clients connect to the WebSocket server (default port: 1234)
2. Clients send messages in the format: `generate | text | filename | channel | time`
3. The server:
   - Parses the message
   - Sources the API key from `elevenlabs-api-key.sh`
   - Runs `generator.py` with the provided parameters
   - Creates a subdirectory for the channel if it doesn't exist
   - Saves the MP3 file to `audio/{channel}/{filename}.mp3`
   - On success, reads existing entries from `events.txt` (in parent directory), checks for duplicates, adds the new entry, sorts all entries by time, and saves back to the file

## Message Format

WebSocket clients should send messages in this format:

```
generate | text | filename | channel | time
```

### Parameters

- **text**: The text to convert to speech
- **filename**: The name of the output file (without .mp3 extension)
- **channel**: The subdirectory where the file will be saved
- **time**: Timestamp for the event log

### Example

```
generate | Hello World | greeting001 | announcements | 2024-01-01T12:00:00
```

This will:
1. Generate speech for "Hello World"
2. Save it as `audio/announcements/greeting001.mp3`
3. Add this line to `events.txt` (in the repository root, sorted by time):
   ```
   2024-01-01T12:00:00|announcements|greeting001.mp3|Hello World
   ```

## Setup

### Prerequisites

- Qt 5 or higher with WebSockets module
- Python 3 with `requests` library
- ElevenLabs API key

### Installation

1. Clone the repository
2. Create the API key script:
   ```bash
   cd audio
   cat > elevenlabs-api-key.sh << 'SCRIPT'
   #!/bin/bash
   export ELEVENLABS_API_KEY="your_api_key_here"
   SCRIPT
   chmod +x elevenlabs-api-key.sh
   ```

3. Build the server:
   ```bash
   cd server
   qmake
   make
   ```

4. Run the server:
   ```bash
   ./solarisserver
   ```

## Testing

### Test generator.py directly:

```bash
cd audio
source elevenlabs-api-key.sh
python3 generator.py "Test text" "testchannel" "testfile"
```

This will create `audio/testchannel/testfile.mp3`.

### Test the WebSocket server:

Use a WebSocket client to connect to `wss://localhost:1234` and send:
```
generate | Hello from WebSocket | test001 | messages | 2024-01-01T12:00:00
```

## File Structure

```
solaris/
├── audio/
│   ├── generator.py              # TTS generator script
│   ├── elevenlabs-api-key.sh     # API key (git-ignored)
│   ├── events.txt                # Event log (git-ignored)
│   └── {channel}/                # Channel subdirectories (git-ignored)
│       └── {filename}.mp3        # Generated audio files
├── server/
│   ├── solarisserver.cpp         # Main server implementation
│   ├── solarisserver.h           # Server header
│   ├── solarisserver.pro         # Qt project file
│   └── main.cpp                  # Entry point
└── client/
    └── performer.html            # (Future client implementation)
```

## Security Notes

- The `elevenlabs-api-key.sh` file is git-ignored for security
- SSL/TLS is required for WebSocket connections
- Certificate and key files must be configured in `SolarisServer::SolarisServer()`

## Events Log Format

Each successful TTS generation adds a line to `events.txt` in the repository root directory. The file is automatically sorted by time, and duplicate entries are prevented:

```
time|channel|filename.mp3|text
```

Example:
```
2024-01-01T12:00:00|announcements|greeting001.mp3|Hello World
2024-01-01T12:05:00|music|intro001.mp3|Welcome to the show
```

**Note**: Entries are automatically sorted chronologically by the time field.
