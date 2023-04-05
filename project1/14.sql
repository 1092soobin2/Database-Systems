SELECT ct.name
FROM Trainer AS tr, City AS ct
WHERE tr.hometown = ct.name
GROUP BY tr.hometown
HAVING count(tr.id) >= ALL (SELECT count(*)
                            FROM Trainer AS tr
                            GROUP BY tr.hometown)
;