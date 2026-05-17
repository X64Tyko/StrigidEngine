#include "AssetRegistry.h"
#include "Json.h"
#include "Logger.h"

#include <filesystem>
#include <fstream>

void AssetRegistry::LoadManifest(const char* contentRoot)
{
	if (!contentRoot || contentRoot[0] == '\0') return;

	std::string dbPath = std::string(contentRoot) + "/AssetDatabase.tnxdb";
	std::ifstream file(dbPath);
	if (!file.is_open())
	{
		LOG_ENG_WARN_F("[AssetRegistry] No manifest found at %s", dbPath.c_str());
		return;
	}

	std::string contents((std::istreambuf_iterator<char>(file)),
	                      std::istreambuf_iterator<char>());
	JsonValue root = JsonParse(contents);
	if (!root.IsObject()) return;

	const JsonValue* assets = root.Find("assets");
	if (!assets || !assets->IsArray()) return;

	size_t count = 0;
	for (const auto& item : assets->AsArray())
	{
		if (!item.IsObject()) continue;

		const JsonValue* uuid = item.Find("uuid");
		const JsonValue* name = item.Find("name");
		const JsonValue* path = item.Find("path");
		const JsonValue* type = item.Find("type");

		if (!uuid || !path || !path->IsString()) continue;

		AssetID id = AssetID::Create(
			static_cast<int64_t>(uuid->AsNumber()),
			type ? static_cast<AssetType>(type->AsInt()) : AssetType::Invalid);

		std::string nameStr = (name && name->IsString()) ? name->AsString() : std::string{};
		if (nameStr.empty())
			nameStr = std::filesystem::path(path->AsString()).stem().string();

		Register(id, TnxName(nameStr.c_str()), path->AsString(), id.GetType());
		++count;
	}

	LOG_ENG_INFO_F("[AssetRegistry] Loaded manifest: %zu assets from %s", count, dbPath.c_str());
}

void AssetRegistry::SetContentRoot(const std::string& root)
{
	ContentRoot = root;
	if (!Entries.empty())
		ValidateAll();
}

void AssetRegistry::Register(const AssetID& id, TnxName name, const std::string& path,
                              AssetType type, uint32_t schemaVersion, AssetFlags flags)
{
	AssetEntry& entry   = Entries[id];
	entry.ID            = id;
	entry.Name          = name;
	entry.Path          = path;
	entry.Type          = type;
	entry.SchemaVersion = schemaVersion;
	entry.Flags         = flags;
	entry.State         = RuntimeFlags::None;

	if (name.IsValid()) NameIndex[entry.Name.Value] = id;

	if (!ContentRoot.empty() && !path.empty())
		Validate(id);
}

bool AssetRegistry::Validate(const AssetID& id)
{
	AssetEntry* e = FindMutableByID(ResolveAlias(id));
	if (!e) return false;

	if (e->Path.empty() || ContentRoot.empty())
		return true;

	const bool exists = std::filesystem::exists(
		std::filesystem::path(ContentRoot) / e->Path);

	if (exists)
	{
		e->State = static_cast<RuntimeFlags>(
			static_cast<uint8_t>(e->State) & ~static_cast<uint8_t>(RuntimeFlags::Missing));
	}
	else
	{
		e->State = static_cast<RuntimeFlags>(
			static_cast<uint8_t>(e->State) | static_cast<uint8_t>(RuntimeFlags::Missing));
		LOG_ENG_WARN_F("[AssetRegistry] Missing: '%s' (%s)", e->Path.c_str(), e->Name.GetStr());
	}

	return exists;
}

void AssetRegistry::ValidateAll()
{
	for (auto& [id, entry] : Entries)
		Validate(id);
}

void AssetRegistry::Unregister(const AssetID& id)
{
	auto it = Entries.find(id);
	if (it == Entries.end()) return;

	const AssetEntry& entry = it->second;
	NameIndex.erase(entry.Name.Value);

	for (auto sit = SlotIndex.begin(); sit != SlotIndex.end(); )
	{
		if (sit->second == id) sit = SlotIndex.erase(sit);
		else ++sit;
	}

	Entries.erase(it);
}