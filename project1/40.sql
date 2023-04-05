SELECT tr.name
FROM CatchedPokemon cp
	JOIN Trainer tr ON tr.id = cp.owner_id
    JOIN Pokemon pkm ON pkm.id = cp.pid
WHERE pkm.name = "Pikachu"
ORDER BY tr.name
;