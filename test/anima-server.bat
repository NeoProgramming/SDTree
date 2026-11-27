@echo off
E:\MySoft\_AI\sdcpp-win11-cuda12\sd-server.exe ^
--listen-ip 127.0.0.1 ^
--listen-port 1234 ^
--diffusion-model "E:\AI\anima\WAI-ANIMA1.safetensors" ^
--vae "E:\AI\anima\qwen_image_vae.safetensors" ^
--llm "E:\AI\anima\qwen_3_06b_base.safetensors" ^
--lora-model-dir "E:\AI\anima\lora"
pause