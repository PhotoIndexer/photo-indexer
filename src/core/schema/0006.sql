-- Faces que falharam porque o disco tinha saído (o original sumiu no meio do
-- trabalho, ou o app ainda o achava conectado ao abrir) viraram erro para
-- sempre. Agora isso devolve a foto à fila (Pipeline: Stage::parked); estas
-- voltam também — as de disco desconectado esperam ele voltar.
UPDATE photo SET faces_state = 'pending', error = '' WHERE faces_state = 'error' AND error = 'Faces: image unreadable';
