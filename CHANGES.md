# Changes Summary - TTS Integration

## Files Modified/Renamed

### Server Files (Renamed)
- `server/sslechoserver.cpp` → `server/solarisserver.cpp`
- `server/sslechoserver.h` → `server/solarisserver.h`
- `server/sslechoserver.pro` → `server/solarisserver.pro`

### Modified Files
- `server/main.cpp` - Updated to use SolarisServer class
- `audio/generator.py` - Added command-line argument support
- `.gitignore` - Added patterns for generated files

### New Files
- `USAGE.md` - Comprehensive documentation

## Key Changes

### 1. Class Rename: SslEchoServer → SolarisServer
All occurrences of `SslEchoServer` have been renamed to `SolarisServer` throughout:
- Header file (#ifndef guards)
- Class definition
- Constructor/destructor
- Method implementations
- Signal connections
- main.cpp instantiation

### 2. Generator.py Enhancements
**New Features:**
- Command-line argument parsing with argparse
- Three required arguments: text, channel, filename
- Automatic channel subdirectory creation
- Deferred API key validation (allows --help to work)

**Usage:**
```bash
python3 generator.py "Text to speak" "channel_name" "output_filename"
```

**Output:**
Creates `audio/channel_name/output_filename.mp3`

### 3. Server TTS Processing (processTextMessage)

**New Message Format:**
```
generate | text | filename | channel | time
```

**Processing Steps:**
1. Parse pipe-delimited message
2. Validate 5-part format
3. Locate audio directory
4. Source API key from `elevenlabs-api-key.sh`
5. Execute generator.py with parameters
6. Wait up to 30 seconds for completion
7. On success, append to events.txt

**Example:**
```
Input:  generate | Hello World | greeting001 | announcements | 2024-01-01T12:00:00
Output: audio/announcements/greeting001.mp3
Log:    2024-01-01T12:00:00|announcements|greeting001.mp3|Hello World
```

### 4. Events Log Format
Each successful generation updates `events.txt` in the repository root directory:
- Reads existing entries
- Checks for duplicate entries (skips if already exists)
- Adds new entry if unique
- Sorts all entries by time field
- Writes sorted entries back to file

Format:
```
time|channel|filename.mp3|text
```

### 5. .gitignore Updates
Added patterns to exclude:
- `audio/*.sh` - API key scripts
- `events.txt` - Event log (in repository root)
- `audio/*/` - All channel subdirectories

## Technical Implementation Details

### Dependencies Added
- `#include <QtCore/QProcess>` - For running generator.py
- `#include <QtCore/QDir>` - For directory operations
- `#include <QtCore/QTextStream>` - For file writing
- Python argparse module

### Error Handling
- Invalid message format detection
- Directory path validation (tries multiple paths)
- Process timeout (30 seconds)
- Exit code checking
- File I/O error handling

### Backward Compatibility
- Non-generate messages still echo to all clients
- Existing WebSocket functionality preserved

## Testing Instructions

### 1. Setup API Key
```bash
cd audio
cat > elevenlabs-api-key.sh << 'SCRIPT'
#!/bin/bash
export ELEVENLABS_API_KEY="your_actual_key_here"
SCRIPT
chmod +x elevenlabs-api-key.sh
```

### 2. Test Generator Directly
```bash
cd audio
source elevenlabs-api-key.sh
python3 generator.py "Test message" "testchannel" "testfile"
# Should create: audio/testchannel/testfile.mp3
```

### 3. Test WebSocket Server
Build and run the server, then send via WebSocket client:
```
generate | Hello from WebSocket | msg001 | greetings | 2024-01-01T12:00:00
```

### 4. Verify Results
- Check file created: `audio/greetings/msg001.mp3`
- Check log entry in: `events.txt` (repository root, sorted by time)

## Security Notes
- API key script is git-ignored
- SSL/TLS certificates required for WebSocket connections
- No authentication on WebSocket connections (should be added in production)

## Future Enhancements
- Add authentication/authorization
- Support for different voices
- Queue management for multiple requests
- Rate limiting
- Error message feedback to clients
- Support for other TTS providers
