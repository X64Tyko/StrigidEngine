#include "AssetRegistry.h"
#include "Logger.h"

#include <filesystem>

void AssetRegistry::SetContentRoot(const std::string& root)
{
	ContentRoot = root;
	if (!Entries.empty())
		ValidateAll();
}

void AssetRegistry::Register(const AssetID& id, const std::string& name, const std::string& path,
                              AssetType type, uint32_t schemaVersion, AssetFlags flags)
{
	AssetEntry& entry   = Entries[id];
	entry.ID            = id;
	entry.Name          = TnxName(name.c_str());
	entry.Path          = path;
	entry.Type          = type;
	entry.SchemaVersion = schemaVersion;
	entry.Flags         = flags;
	entry.State         = RuntimeFlags::None;

	if (!name.empty()) NameIndex[entry.Name.Value] = id;

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