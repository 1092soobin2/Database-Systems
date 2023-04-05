SELECT DISTINCT tr.name
FROM Trainer AS tr
	JOIN CatchedPokemon AS cp ON tr.id = cp.owner_id
WHERE cp.id <> ANY (SELECT cp2.id
                     FROM CatchedPokemon AS cp2
                     WHERE cp2.owner_id = cp.owner_id
                    	AND cp2.pid = cp.pid)
ORDER BY tr.name
;