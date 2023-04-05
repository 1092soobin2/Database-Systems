SELECT nickname
FROM CatchedPokemon AS cp
WHERE cp.level >= 40
	AND owner_id >= 5
ORDER BY nickname
;