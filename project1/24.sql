SELECT pkm.name
FROM Pokemon AS pkm
WHERE pkm.name LIKE "A%"
	OR pkm.name LIKE "E%"
    OR pkm.name LIKE "I%"
    OR pkm.name LIKE "O%"
    OR pkm.name LIKE "U%"
ORDER BY pkm.name
;