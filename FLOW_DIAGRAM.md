# System Flow Diagram

## Complete Message Processing Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                     WebSocket Client                             │
│                                                                  │
│  Sends: "generate | Hello World | file1 | news | 2024-01-01"   │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│                   SolarisServer (C++/Qt)                         │
│  Port: 1234 (Secure WebSocket)                                  │
│                                                                  │
│  1. Receives message via processTextMessage()                   │
│  2. Checks: message.startsWith("generate|") or "generate |"     │
│  3. Splits by "|" delimiter                                     │
│  4. Validates 5 parts: [generate, text, filename, channel, time]│
│  5. Extracts:                                                   │
│     • text = "Hello World"                                      │
│     • filename = "file1"                                        │
│     • channel = "news"                                          │
│     • time = "2024-01-01"                                       │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│                  Command Execution (QProcess)                    │
│                                                                  │
│  bash -c 'source audio/elevenlabs-api-key.sh &&                │
│           python3 audio/generator.py                            │
│                   "Hello World"                                 │
│                   "news"                                        │
│                   "file1"'                                      │
│                                                                  │
│  • Sets ELEVENLABS_API_KEY environment variable                 │
│  • Timeout: 30 seconds                                          │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│                 generator.py (Python)                            │
│                                                                  │
│  1. Parses arguments:                                           │
│     • text = "Hello World"                                      │
│     • channel = "news"                                          │
│     • filename = "file1"                                        │
│                                                                  │
│  2. Creates directory: audio/news/ (if not exists)              │
│                                                                  │
│  3. Calls ElevenLabs API:                                       │
│     POST https://api.elevenlabs.io/v1/text-to-speech/{voice}   │
│     Headers: xi-api-key: $ELEVENLABS_API_KEY                    │
│     Body: { text: "Hello World", ... }                          │
│                                                                  │
│  4. Saves response to: audio/news/file1.mp3                     │
│                                                                  │
│  5. Exits with code 0 (success)                                 │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│              SolarisServer - Post-Processing                     │
│                                                                  │
│  1. Checks exit code == 0                                       │
│                                                                  │
│  2. Opens audio/events.txt (append mode)                        │
│                                                                  │
│  3. Writes line:                                                │
│     "2024-01-01|news|file1.mp3|Hello World\n"                   │
│                                                                  │
│  4. Logs success message                                        │
│                                                                  │
│  5. Echoes original message to all WebSocket clients            │
└─────────────────────────────────────────────────────────────────┘
```

## File System State After Processing

```
audio/
├── elevenlabs-api-key.sh      # Contains: export ELEVENLABS_API_KEY="..."
├── generator.py               # Python TTS script
├── events.txt                 # Event log
│   └── Contains: "2024-01-01|news|file1.mp3|Hello World"
│
└── news/                      # Channel directory (created automatically)
    └── file1.mp3              # Generated audio file
```

## Error Handling

### Invalid Message Format
```
Input:  "generate | Hello | file1"  (only 3 parts)
Action: Log warning, no processing
Output: Original message echoed to clients
```

### Generator Failure
```
Input:  Valid format, but API call fails
Action: Log error, check exit code != 0
Output: No events.txt entry, error logged
```

### Timeout
```
Input:  Valid format, but takes > 30 seconds
Action: Process killed, timeout logged
Output: No events.txt entry, error logged
```

## Message Parameter Mapping

| Position | WebSocket Message | Generator.py Arg | Events.txt Position |
|----------|------------------|------------------|---------------------|
| 0        | "generate"       | (not used)       | (not used)         |
| 1        | text             | arg[0]           | position 4         |
| 2        | filename         | arg[2]           | position 3 (+.mp3) |
| 3        | channel          | arg[1]           | position 2         |
| 4        | time             | (not used)       | position 1         |

## Security Considerations

1. **API Key Protection**
   - Stored in `elevenlabs-api-key.sh` (git-ignored)
   - Sourced only during generation
   - Never transmitted to clients

2. **Input Sanitization**
   - Message parts trimmed of whitespace
   - Validated for correct count (5 parts)
   - Command arguments properly quoted

3. **File System Security**
   - Channel directories created with standard permissions
   - Files written only to audio/ subdirectories
   - No path traversal possible (relative to audio/)

4. **Network Security**
   - SSL/TLS required for WebSocket
   - Certificate verification in place
   - Secure mode enforced

## Performance Notes

- **Typical generation time:** 2-5 seconds
- **Maximum wait time:** 30 seconds (timeout)
- **Concurrent requests:** Not queued (immediate processing)
- **File size:** ~50-200KB per 10 seconds of speech
