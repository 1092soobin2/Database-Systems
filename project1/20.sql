SELECT tr.name
FROM Trainer AS tr, Gym AS g
WHERE tr.id = g.leader_id
ORDER BY tr.name
;