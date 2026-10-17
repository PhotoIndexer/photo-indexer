-- Nova regra das categorias da IA (core/Classifier.h: "nenhuma", irmãs, o
-- grupo na dúvida, limiar 0,45) e frases revistas (Taxonomy.cpp). As fotos
-- já categorizadas voltam para o Categorizer: ele usa o embedding guardado
-- (photo_embedding), então é só refazer a conta — nenhuma imagem é reaberta.
-- As categorias postas ou tiradas pela pessoa (source user/rejected) ficam.
UPDATE photo SET ai_state = 'pending' WHERE ai_state IN ('done', 'error') AND private = 0;
