-- ============================================================================
-- AUREA - contraintes OPTIONNELLES (changent le comportement actuel : a valider avant de les activer)
-- Dans SQLite, ces liens n'etaient pas declares : l'application ne les verifiait pas.
-- ============================================================================

-- 1) Une tache de production affectee a un employe : si l'employe est supprime, la tache reste sans employe
--    (meme regle que employee_attendance / employee_assignments).
ALTER TABLE tache ADD CONSTRAINT fk_tache_employe
  FOREIGN KEY (id_employe) REFERENCES employees(id) ON DELETE SET NULL;
