@echo off
E:\MySoft\_AI\sdcpp-win11-cuda12\sd-server.exe ^
--listen-ip 127.0.0.1 ^
--listen-port 1234 ^
--diffusion-model "E:\AI\chroma\Chroma1-HD-Q6_K.gguf" ^
--vae "E:\AI\chroma\ae.safetensors" ^
--t5xxl "E:\AI\chroma\t5xxl_fp8_e4m3fn.safetensors"
pause