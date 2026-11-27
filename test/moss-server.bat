@echo off
E:\MySoft\_AI\sdcpp-win11-cuda12\sd-server.exe ^
--listen-ip 127.0.0.1 ^
--listen-port 1234 ^
--diffusion-model "E:\AI\moss\moss_image_sft_int8_convrot.safetensors" ^
--vae "E:\AI\moss\flux2-vae.safetensors" ^
--llm "E:\AI\moss\qwen3.5_9b_int8_convrot.safetensors" ^
--tokenizer "E:\AI\moss\tokenizer.json" ^
--lora-model-dir "E:\AI\moss\lora"
pause