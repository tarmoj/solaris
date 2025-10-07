# audio_generator.py
import requests
import os

ELEVENLABS_API_KEY = os.environ.get("ELEVENLABS_API_KEY")  # store key in env var
print(f"Using ElevenLabs API Key: { ELEVENLABS_API_KEY}")  # print first 4 chars for verification
if not ELEVENLABS_API_KEY:
    raise RuntimeError("Set ELEVENLABS_API_KEY environment variable")

VOICE_ID = "cgSgspJ2msm6clMCkdW9"  # Jessica's voice ID

def generate_audio(text: str, output_path: str):
    """Generate speech from text and save as MP3 file."""
    url = f"https://api.elevenlabs.io/v1/text-to-speech/{VOICE_ID}"

    headers = {
        "Accept": "audio/mpeg",
        "xi-api-key": ELEVENLABS_API_KEY,
        "Content-Type": "application/json"
    }

    data = {
        "text": text,
        "model_id": "eleven_multilingual_v2",
        "voice_settings": {
            "stability": 0.5,
            "similarity_boost": 0.8
        }
    }

    response = requests.post(url, headers=headers, json=data)
    response.raise_for_status()

    with open(output_path, "wb") as f:
        f.write(response.content)

    print(f"✅ Saved: {output_path}")
    
if __name__ == "__main__":
    print("Generating audio... ")
    sample_text = "Hello, this is a test of the ElevenLabs text-to-speech API."
    generate_audio(sample_text, "output.mp3")# ai-chat.py

 
