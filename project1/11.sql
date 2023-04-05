SELECT cp.nickname
FROM CatchedPokemon cp
	JOIN Pokemon pkm ON cp.pid = pkm.id
    JOIN Trainer tr ON tr.id = cp.owner_id
    JOIN Gym g ON g.leader_id = tr.id
WHERE g.city = "Sangnok City"
	AND pkm.type = "water"
ORDER BY cp.nickname
;