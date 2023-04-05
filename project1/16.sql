SELECT tr.name
FROM Trainer AS tr
	JOIN Gym AS g ON tr.id = g.leader_id
    JOIN City AS ct ON g.city = ct.name
WHERE ct.description = "Amazon"
;