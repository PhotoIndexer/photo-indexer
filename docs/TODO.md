# TODO

Pendências conhecidas, em ordem de prioridade.

## Validação nos alvos

- [ ] Rodar `make dmg-all`, `make windows` e o `windows-native` (arm64) e
      conferir os pacotes: o LibRaw e o `Disks.cpp` (DiskArbitration no macOS,
      Win32 no Windows) só foram compilados e testados no Linux.
- [ ] Conferir a identidade de disco num Mac com volume externo (APFS/exFAT),
      num Windows com unidade mapeada de rede e num Linux com CIFS montado.
- [ ] Conferir RAW de verdade (NEF, CR2, CR3, ARW, RAF, DNG): prévia embutida,
      orientação e o hash do sensor. Os testes não trazem RAW (tamanho).

## Produto

- [ ] HEIC/AVIF: o Qt base não tem plugin; incluir o `qtimageformats` nos
      builders (os seis alvos) ou ler a prévia embutida.
- [ ] Vídeos (MOV/MP4): catalogar com um quadro como miniatura.
- [ ] Mapa das fotos com GPS (aba Lugares).
- [ ] Busca por IA multilíngue (o CLIP só entende inglês).
- [ ] Exportar/copiar a seleção para uma pasta.
- [ ] Detectar rostos em RAW pela prévia de maior resolução quando a menor
      for pequena demais.

## Escala

- [ ] Índice de embeddings do CLIP em memória (float32): ~200 MB a cada 100
      mil fotos na primeira busca por IA. Avaliar float16 ou int8.
- [ ] `FaceIndex` é busca exata; acima de ~1 milhão de faces, considerar um
      índice aproximado.
