-- MySQL dump 10.13  Distrib 8.0.16, for Win64 (x86_64)
--
-- Host: 127.0.0.1    Database: ember_login
-- ------------------------------------------------------
-- Server version	8.0.16

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
 SET NAMES utf8 ;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `architectures`
--

DROP TABLE IF EXISTS `architectures`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
 SET character_set_client = utf8mb4 ;
CREATE TABLE `architectures` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `value` varchar(45) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `name_UNIQUE` (`value`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `bans`
--

DROP TABLE IF EXISTS `bans`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
 SET character_set_client = utf8mb4 ;
CREATE TABLE `bans` (
  `user_id` int(10) unsigned NOT NULL,
  `by_id` int(10) unsigned NOT NULL,
  `date` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `reason` longtext NOT NULL,
  PRIMARY KEY (`user_id`),
  KEY `banned_by_idx` (`by_id`),
  KEY `banned_user_idx` (`user_id`),
  CONSTRAINT `banned_by_id` FOREIGN KEY (`by_id`) REFERENCES `users` (`id`),
  CONSTRAINT `banned_user_id` FOREIGN KEY (`user_id`) REFERENCES `users` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `characters`
--

DROP TABLE IF EXISTS `characters`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
 SET character_set_client = utf8mb4 ;
CREATE TABLE `characters` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `account_id` int(11) unsigned NOT NULL,
  `realm_id` int(11) unsigned NOT NULL,
  `name` varchar(45) NOT NULL,
  `internal_name` varchar(45) NOT NULL,
  `deletion_date` datetime DEFAULT NULL COMMENT 'Deletion date is stored here and kept as NULL in most cases to allow for a unique constraint to be kept character names. All names on a realm must be unique for non-deleted characters but multiple deleted characters may have the same name.',
  `race` tinyint(4) NOT NULL,
  `class` tinyint(4) NOT NULL,
  `gender` tinyint(4) NOT NULL,
  `skin` tinyint(4) NOT NULL,
  `face` tinyint(4) NOT NULL,
  `hairstyle` tinyint(4) NOT NULL,
  `haircolour` tinyint(4) NOT NULL,
  `facialhair` tinyint(4) NOT NULL,
  `level` tinyint(4) NOT NULL,
  `zone` int(11) NOT NULL,
  `map` int(11) NOT NULL,
  `x` float NOT NULL,
  `y` float NOT NULL,
  `z` float NOT NULL,
  `o` float NOT NULL,
  `flags` int(11) NOT NULL,
  `first_login` varchar(45) NOT NULL,
  `pet_display` varchar(45) NOT NULL,
  `pet_level` varchar(45) NOT NULL,
  `pet_family` varchar(45) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `char_name_unique` (`realm_id`,`deletion_date`,`internal_name`),
  KEY `account_ref_idx` (`account_id`),
  KEY `realm_ref_idx` (`realm_id`),
  CONSTRAINT `account_ref` FOREIGN KEY (`account_id`) REFERENCES `users` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `realm_ref` FOREIGN KEY (`realm_id`) REFERENCES `realms` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=27 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `guild_characters`
--

DROP TABLE IF EXISTS `guild_characters`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
 SET character_set_client = utf8mb4 ;
CREATE TABLE `guild_characters` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `character_id` int(11) unsigned NOT NULL,
  `guild_id` int(11) unsigned NOT NULL,
  `rank` int(10) unsigned NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `character_ref_idx` (`character_id`),
  KEY `guild_ref_idx` (`guild_id`),
  CONSTRAINT `character_ref` FOREIGN KEY (`character_id`) REFERENCES `characters` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `guild_ref` FOREIGN KEY (`guild_id`) REFERENCES `guilds` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `guilds`
--

DROP TABLE IF EXISTS `guilds`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
 SET character_set_client = utf8mb4 ;
CREATE TABLE `guilds` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `realm_id` int(10) unsigned NOT NULL,
  `name` varchar(45) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `unique_name` (`realm_id`,`name`),
  KEY `realm_ref2_idx` (`realm_id`),
  CONSTRAINT `realm_ref2` FOREIGN KEY (`realm_id`) REFERENCES `realms` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `ip_bans`
--

DROP TABLE IF EXISTS `ip_bans`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
 SET character_set_client = utf8mb4 ;
CREATE TABLE `ip_bans` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `ip` varchar(45) NOT NULL,
  `cidr` int(11) NOT NULL,
  `reason` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `locales`
--

DROP TABLE IF EXISTS `locales`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
 SET character_set_client = utf8mb4 ;
CREATE TABLE `locales` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `value` varchar(45) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `name_UNIQUE` (`value`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `operating_systems`
--

DROP TABLE IF EXISTS `operating_systems`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
 SET character_set_client = utf8mb4 ;
CREATE TABLE `operating_systems` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `value` varchar(45) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `name_UNIQUE` (`value`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `patches`
--

DROP TABLE IF EXISTS `patches`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
 SET character_set_client = utf8mb4 ;
CREATE TABLE `patches` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(255) NOT NULL,
  `locale` int(10) unsigned DEFAULT NULL,
  `architecture` int(10) unsigned DEFAULT NULL,
  `os` int(10) unsigned DEFAULT NULL,
  `from` int(10) unsigned NOT NULL,
  `to` int(10) unsigned NOT NULL,
  `md5` varchar(255) DEFAULT NULL,
  `size` int(11) DEFAULT '0',
  `mpq` bit(1) NOT NULL DEFAULT b'0',
  `rollup` bit(1) NOT NULL DEFAULT b'0',
  PRIMARY KEY (`id`),
  UNIQUE KEY `from_to_unique` (`from`,`to`),
  KEY `locale_fk_idx` (`locale`),
  KEY `os_fk_idx` (`os`),
  KEY `arch_fk_idx` (`architecture`),
  CONSTRAINT `arch_fk` FOREIGN KEY (`architecture`) REFERENCES `architectures` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `locale_fk` FOREIGN KEY (`locale`) REFERENCES `locales` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `os_fk` FOREIGN KEY (`os`) REFERENCES `operating_systems` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `realms`
--

DROP TABLE IF EXISTS `realms`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
 SET character_set_client = utf8mb4 ;
CREATE TABLE `realms` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(45) NOT NULL,
  `ip` varchar(45) NOT NULL,
  `type` int(11) NOT NULL,
  `flags` int(11) NOT NULL,
  `population` float NOT NULL,
  `creation_setting` tinyint(3) unsigned NOT NULL,
  `category` int(11) NOT NULL DEFAULT '1',
  `region` int(11) NOT NULL DEFAULT '1',
  `port` int(10) unsigned NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=2 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `schema_history`
--

DROP TABLE IF EXISTS `schema_history`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
 SET character_set_client = utf8mb4 ;
CREATE TABLE `schema_history` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `core_version` varchar(45) NOT NULL,
  `commit` varchar(45) NOT NULL,
  `install_date` datetime NOT NULL,
  `installed_by` varchar(45) NOT NULL,
  `file` varchar(255) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `survey_results`
--

DROP TABLE IF EXISTS `survey_results`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
 SET character_set_client = utf8mb4 ;
CREATE TABLE `survey_results` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `survey_id` int(10) unsigned NOT NULL,
  `data` text,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=6 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `suspensions`
--

DROP TABLE IF EXISTS `suspensions`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
 SET character_set_client = utf8mb4 ;
CREATE TABLE `suspensions` (
  `user_id` int(10) unsigned NOT NULL,
  `by_id` int(10) unsigned NOT NULL,
  `start_date` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `end_date` datetime NOT NULL,
  `reason` longtext NOT NULL,
  PRIMARY KEY (`user_id`),
  KEY `suspended_by_idx` (`by_id`),
  CONSTRAINT `suspended_by` FOREIGN KEY (`by_id`) REFERENCES `users` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `suspended_user` FOREIGN KEY (`user_id`) REFERENCES `users` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `users`
--

DROP TABLE IF EXISTS `users`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
 SET character_set_client = utf8mb4 ;
CREATE TABLE `users` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `username` varchar(255) NOT NULL,
  `s` binary(32) NOT NULL,
  `v` varchar(255) NOT NULL,
  `creation_date` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `subscriber` bit(1) NOT NULL DEFAULT b'1',
  `survey_request` bit(1) NOT NULL DEFAULT b'0',
  `pin_method` int(11) NOT NULL DEFAULT '0',
  `pin` int(11) DEFAULT NULL,
  `totp_key` varchar(45) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `username_UNIQUE` (`username`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2024-11-15  4:33:21
