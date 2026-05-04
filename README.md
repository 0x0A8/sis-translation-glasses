# 🕶️ Project Lens
> Smart glasses with real-time multilingual translation — breaking language barriers, one conversation at a time.

![Status](https://img.shields.io/badge/status-active-brightgreen)
![Type](https://img.shields.io/badge/type-hardware%20%2B%20ML-blue)
![Version](https://img.shields.io/badge/version-v0.1--planning-orange)

---

## About

Project Lens is a senior independent study building wearable smart glasses capable of translating live conversations across languages. The system combines embedded hardware, speech recognition, neural machine translation, and audio output — enabling two people speaking different languages to communicate fluidly without picking up their phones.

---

## Core Features

| Feature | Description |
|---|---|
| 🎙️ Live STT | Continuous speech-to-text from integrated microphones |
| 🌐 Neural Translation | Low-latency multilingual translation at the edge or via API |
| 🔊 Audio Output | Translated speech via bone conduction or earpiece |
| 👓 Minimal UX | HUD overlay or audio-only — no hands required |

---

## Planned Stack

- **Hardware:** ESP32-S3
- **Speech-to-Text:** OpenAI Whisper (edge)
- **Translation:** Helsinki NLP / Meta NLLB
- **Runtime:** Python, TFLite / ONNX
- **Audio Input:** Two I2S MEMS Microphones
- **Audio Output:** Bone conduction transducer
- **Frame:** Standard Glasses

---

## Milestones

- [ ] **Phase 1** — Project setup, repo structure, literature review
- [ ] **Phase 2** — Hardware prototype v1 (frame + mic + speaker)
- [ ] **Phase 3** — STT + translation pipeline with latency benchmarks
- [ ] **Phase 4** — Integration into wearable form factor
- [ ] **Phase 5** — Live demo & final SIS presentation

---

*Senior Independent Study · 2026*
