SELECT tr.name
FROM Trainer AS tr
	JOIN Gym AS g ON g.leader_id = tr.id
WHERE g.city = "Brown City"
;