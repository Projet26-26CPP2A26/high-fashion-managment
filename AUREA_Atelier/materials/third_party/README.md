# Dépendances QR embarquées

Les sources sont incluses dans ce projet : aucune connexion réseau n’est nécessaire pour compiler ou utiliser les QR codes.

- **QR-Code-generator**, Nayuki : encodeur C++ sous licence MIT. Le texte de licence est conservé dans `qrcodegen.cpp` et `qrcodegen.hpp`. Source : https://github.com/nayuki/QR-Code-generator
- **quirc**, Daniel Beer et contributeurs : décodeur C sous licence ISC. Licence conservée dans `quirc/LICENSE` et en tête des fichiers. Source : https://github.com/dlbeer/quirc

Les commits téléchargés sont consignés dans `versions.json`. Ces dépendances identifient/génèrent les codes ; la validation du lot et les mouvements de stock restent dans le code métier AUREA.
