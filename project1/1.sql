SELECT tr.name
FROM CatchedPokemon AS cp
	JOIN Trainer AS tr ON cp.owner_id = tr.id
GROUP BY cp.owner_id
HAVING COUNT(cp.pid) >= 3
ORDER BY COUNT(cp.pid)
;