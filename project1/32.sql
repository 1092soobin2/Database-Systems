SELECT tr.name, pkm.name, COUNT(*)
FROM Trainer AS tr, CatchedPokemon AS cp, Pokemon AS pkm
WHERE tr.id = cp.owner_id
	AND cp.pid = pkm.id
    AND cp.owner_id IN (SELECT cp2.owner_id
                        FROM CatchedPokemon AS cp2
                        	JOIN Pokemon AS pkm2 ON pkm2.id = cp2.pid
                        GROUP BY cp2.owner_id
                        HAVING COUNT(DISTINCT pkm2.type) = 1)
GROUP BY tr.id, pkm.name
ORDER BY tr.name
;