/*
  ==============================================================================

	KeyGen.h
	Created: 5 Jun 2024 6:55:15pm
	Author:  mpue

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <ctime>
#include <vector>
#include <functional>
#include <regex>
#include <random>
#include <algorithm>

class KeyGen {
public:
	std::string generateKey(const std::string& email) {
		if (!isValidEmail(email)) {
			throw std::invalid_argument("Invalid email address.");
		}

		std::vector<int> keyParts = generateKeyPartsFromEmail(email);
		return formatKey(keyParts);
	}

	bool isValidKey(const std::string& key, const std::string& email) {
		if (!isValidEmail(email) || key.length() != 19) {
			return false;
		}

		std::string expectedKey = generateKey(email);

		// Logging for debugging
		DBG("KeyGen::isValidKey - Expected Key: " + expectedKey);
		DBG("KeyGen::isValidKey - Provided Key: " + key);

		return key == expectedKey;
	}

private:
	std::vector<int> generateKeyPartsFromEmail(const std::string& email) {
		std::vector<int> parts(16);
		std::hash<std::string> hash_fn;
		size_t email_hash = hash_fn(email);

		// Logging for debugging
		DBG("KeyGen::generateKeyPartsFromEmail - Email Hash: " + std::to_string(email_hash));

		std::mt19937 gen(static_cast<unsigned int>(email_hash)); // Seed random generator with email hash
		std::uniform_int_distribution<> dis(0, 15);

		for (int& part : parts) {
			part = dis(gen);
		}

		for (int& part : parts) {
			part = (part + 7) % 16;
		}

		// Logging for debugging
		std::stringstream partsStream;
		for (const int& part : parts) {
			partsStream << std::hex << std::uppercase << part << " ";
		}
		DBG("KeyGen::generateKeyPartsFromEmail - Key Parts: " + partsStream.str());

		return parts;
	}

	std::string formatKey(const std::vector<int>& parts) {
		std::stringstream key;
		for (size_t i = 0; i < parts.size(); ++i) {
			if (i > 0 && i % 4 == 0) {
				key << "-";
			}
			key << std::hex << std::uppercase << parts[i];
		}

		// Logging for debugging
		DBG("KeyGen::formatKey - Formatted Key: " + key.str());

		return key.str();
	}

	bool isValidEmail(const std::string& email) {
		const std::regex pattern(
			R"(([\w\.-]+)@([\w\.-]+)\.([a-zA-Z]{2,}))");
		return std::regex_match(email, pattern);
	}
};