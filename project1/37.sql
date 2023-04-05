SELECT SUM(cp.level)
FROM CatchedPokemon AS cp
	JOIN Pokemon AS pkm ON cp.pid = pkm.id
WHERE type = "fire"
;