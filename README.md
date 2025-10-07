# solaris
Software for performance piece "Solaris" by Andrus Kallastu &amp; Co.

## Features

- **Secure WebSocket Server** - SSL/TLS enabled WebSocket server for real-time communication
- **Text-to-Speech Integration** - Generate audio using ElevenLabs API
- **Channel-based Organization** - Automatic organization of audio files by channel
- **Event Logging** - Comprehensive logging of all TTS generations

## Quick Start

See [USAGE.md](USAGE.md) for detailed setup and usage instructions.

## Documentation

- **[USAGE.md](USAGE.md)** - Setup, configuration, and usage guide
- **[CHANGES.md](CHANGES.md)** - Technical implementation details
- **[FLOW_DIAGRAM.md](FLOW_DIAGRAM.md)** - System architecture and flow
- **[VALIDATION.md](VALIDATION.md)** - Implementation verification checklist

## Components

### Server (C++/Qt)
- **SolarisServer** - WebSocket server with TTS integration
- Listens on port 1234 (configurable)
- Processes messages in format: `generate | text | filename | channel | time`

### Audio Generator (Python)
- **generator.py** - ElevenLabs TTS integration
- Command-line interface for text-to-speech generation
- Automatic channel directory management

## License

See [LICENSE](LICENSE) file for details.
