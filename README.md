# AeroTerra Smart Cooling System

AeroTerra est un système de refroidissement intelligent, éco-responsable et entièrement automatisé conçu autour d'un microcontrôleur ESP32. Ce projet a été développé dans le cadre du concours Beat the Heat.

## Principe de fonctionnement
Le système repose sur le principe thermodynamique du refroidissement par évaporation(climatisation adiabatique). Un ventilateur pulse l'air chaud ambiant à travers un milieu poreux naturel constitué de bâtonnets de cannelle, constamment humidifié par une mini-pompe à eau. En s'évaporant, l'eau absorbe la chaleur du flux d'air, restituant un air frais, purifié et agréablement parfumé.

## Fonctionnalités clés
* Suivi thermique en temps réel : Mesure précise de la température via un capteur DS18B20.
* Automatisation intelligente : Contrôle automatique des actionneurs (pompe et ventilateur) selon des seuils de température prédéfinis.
* Supervision IoT : Connectivité Wi-Fi intégrée permettant la transmission et l'affichage des données sur une interface visuelle (ordinateur ou smartphone).

##  Composants principaux
* Microcontrôleur : ESP32
* Capteur : DS18B20 (Température)
* Actionneur : Mini-pompe à eau (12V), Ventilateur de récupération 12V
* Alimentation & Puissance : Module régulateur LM2596, Transistors BC548, Diode de protection 1N4007
* Matériaux structurels : Boîtier transparent, plaques de Forex PVC, bâtonnets de cannelle

