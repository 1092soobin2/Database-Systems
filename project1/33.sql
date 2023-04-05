SELECT tr.name
FROM CatchedPokemon AS cp
	JOIN Trainer AS tr ON tr.id = cp.owner_id
    JOIN Pokemon AS pkm ON pkm.id = cp.pid
WHERE pkm.type = "Psychic"
ORDER BY tr.name
;