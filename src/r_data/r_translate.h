#ifndef __R_TRANSLATE_H
#define __R_TRANSLATE_H

#include "doomtype.h"
#include "tarray.h"

class FSerializer;

enum
{
	TRANSLATION_Invalid,
	TRANSLATION_Players,
	TRANSLATION_PlayersExtra,
	TRANSLATION_Standard,
	TRANSLATION_LevelScripted,
	TRANSLATION_Decals,
	TRANSLATION_PlayerCorpses,
	TRANSLATION_Decorate,
	TRANSLATION_Blood,
	TRANSLATION_RainPillar,
	TRANSLATION_Custom,
	TRANSLATION_Font,

	NUM_TRANSLATION_TABLES
};

enum EStandardTranslations
{
	STD_Ice = 7,
	STD_Gray = 8,		// a 0-255 gray ramp
	STD_Grayscale = 9,	// desaturated version of the palette.
};

struct FRemapTable;

class FUniquePalette
{
	friend struct FRemapTable;
	struct PalData
	{
		int crc32;
		PalEntry pe[256];
	};
	static TArray<PalData> AllPalettes;

	int Index;
	FRemapTable *remap;

	FUniquePalette(FRemapTable *r) { remap = r; Index = -1; }

public:

	static PalEntry *GetPalette(unsigned int index)
	{
		return index > 0 && index <= AllPalettes.Size() ? AllPalettes[index - 1].pe : NULL;
	}
	bool Update();
	int GetIndex() const { return Index; }
};


struct FRemapTable
{
	FRemapTable(int count=256);
	FRemapTable(const FRemapTable &o);
	~FRemapTable();

	FRemapTable &operator= (const FRemapTable &o);
	bool operator==(const FRemapTable &o);
	void MakeIdentity();
	void KillNative();
	void UpdateNative();
	bool IsIdentity() const;
	void Serialize(FSerializer &arc);
	static void StaticSerializeTranslations(FSerializer &arc);
	bool AddIndexRange(int start, int end, int pal1, int pal2);
	bool AddColorRange(int start, int end, int r1,int g1, int b1, int r2, int g2, int b2);
	bool AddDesaturation(int start, int end, double r1, double g1, double b1, double r2, double g2, double b2);
	bool AddColourisation(int start, int end, int r, int g, int b);
	bool AddTint(int start, int end, int r, int g, int b, int amount);
	bool AddToTranslation(const char * range);
	bool AddColors(int start, int count, const uint8_t*);
	int StoreTranslation(int slot);
	int GetUniqueIndex();

	uint8_t *Remap;				// For the software renderer
	PalEntry *Palette;			// The ideal palette this maps to
	FUniquePalette *Native;		// The index into the list of unique palettes (this is to avoid frequent texture recreation with changing ACS translations)
	int NumEntries;				// # of elements in this table (usually 256)
	bool Inactive;				// This table is inactive and should be treated as if it was passed as NULL

private:
	void Free();
	void Alloc(int count);
};

// A class that initializes unusued pointers to NULL. This is used so that when
// the TAutoGrowArray below is expanded, the new elements will be NULLed.
class FRemapTablePtr
{
public:
	FRemapTablePtr() throw() : Ptr(0) {}
	FRemapTablePtr(FRemapTable *p) throw() : Ptr(p) {}
	FRemapTablePtr(const FRemapTablePtr &p) throw() : Ptr(p.Ptr) {}
	operator FRemapTable *() const throw() { return Ptr; }
	FRemapTablePtr &operator= (FRemapTable *p) throw() { Ptr = p; return *this; }
	FRemapTablePtr &operator= (FRemapTablePtr &p) throw() { Ptr = p.Ptr; return *this; }
	FRemapTable &operator*() const throw() { return *Ptr; }
	FRemapTable *operator->() const throw() { return Ptr; }
private:
	FRemapTable *Ptr;
};

extern TAutoGrowArray<FRemapTablePtr, FRemapTable *> translationtables[NUM_TRANSLATION_TABLES];

#define TRANSLATION_SHIFT 16
#define TRANSLATION_MASK ((1<<TRANSLATION_SHIFT)-1)
#define TRANSLATIONTYPE_MASK (255<<TRANSLATION_SHIFT)

inline uint32_t TRANSLATION(uint8_t a, uint32_t b)
{
	return (a<<TRANSLATION_SHIFT) | b;
}
inline int GetTranslationType(uint32_t trans)
{
	return (trans&TRANSLATIONTYPE_MASK) >> TRANSLATION_SHIFT;
}
inline int GetTranslationIndex(uint32_t trans)
{
	return (trans&TRANSLATION_MASK);
}
// Retrieve the FRemapTable that an actor's translation value maps to.
FRemapTable *TranslationToTable(int translation);

#define MAX_ACS_TRANSLATIONS		65535
#define MAX_DECORATE_TRANSLATIONS	65535

// Initialize color translation tables, for player rendering etc.
void R_InitTranslationTables (void);
void R_DeinitTranslationTables();

void R_BuildPlayerTranslation (int player);		// [RH] Actually create a player's translation table.
void R_GetPlayerTranslation (int color, const struct FPlayerColorSet *colorset, class FPlayerSkin *skin, struct FRemapTable *table);

extern const uint8_t IcePalette[16][3];

int CreateBloodTranslation(PalEntry color);

int R_FindCustomTranslation(FName name);
void R_ParseTrnslate();

struct TextureManipulation
{
	enum
	{
		BlendNone = 0,
		BlendAlpha = 1,
		BlendScreen = 2,
		BlendOverlay = 3,
		BlendHardLight = 4,
		BlendMask = 7,
		InvertBit = 8,
		ActiveBit = 16,	// Must be set for the shader to do something
	};
	PalEntry AddColor;		// Alpha contains the blend flags
	PalEntry ModulateColor;	// Alpha may contain a multiplier to get higher values than 1.0 without promoting this to 4 full floats.
	PalEntry BlendColor;
	float DesaturationFactor;

	bool CheckIfEnabled()	// check if this manipulation is doing something. NoOps do not need to be preserved, unless they override older setttings.
	{
		if (AddColor != 0 ||	// this includes a check for the blend mode without which BlendColor is not active
			ModulateColor != 0x01ffffff ||	// 1 in alpha must be the default for a no-op.
			DesaturationFactor != 0)
		{
			AddColor.a |= ActiveBit; // mark as active for the shader's benefit.
			return true;
		}
		return false;
	}

	void SetTextureModulateColor(int slot, PalEntry rgb)
	{
		rgb.a = ModulateColor.a;
		ModulateColor = rgb;
	}

	void SetTextureModulateScaleFactor(int slot, int fac)
	{
		ModulateColor.a = (uint8_t)fac;
	}

	void SetTextureAdditiveColor(int slot, PalEntry rgb)
	{
		rgb.a = AddColor.a;
		AddColor = rgb;
	}

	void SetTextureBlendColor(int slot, PalEntry rgb)
	{
		BlendColor = rgb;
	}

	void SetTextureDesaturationFactor(int slot, double fac)
	{
		DesaturationFactor = (float)fac;
	}

	void SetTextureBlendMode(int slot, int mode)
	{
		AddColor.a = (AddColor.a & ~TextureManipulation::BlendMask) | (mode & TextureManipulation::BlendMask);
	}

	void SetTextureInvert(bool on)
	{
		AddColor.a |= TextureManipulation::InvertBit;
		AddColor.a &= ~TextureManipulation::InvertBit;
	}

};




#endif // __R_TRANSLATE_H
