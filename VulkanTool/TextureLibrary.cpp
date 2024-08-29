#include "TextureLibrary.h"
#include <regex>
#include "../VulkanTool/stb_image.h"
#include <sstream>

namespace GAME {

	template <typename T>
	T TextureConverter(const std::string& s) {
		try {
			T v{};
			std::istringstream _{ s };
			_.exceptions(std::ios::failbit);
			_ >> v;
			return v;
		}
		catch (std::exception& e) {
			throw std::runtime_error("cannot parse value '" + s + "' to type<T>.");
		};
	}

	void LeaveOnlyLetters(std::string name, const char Char, GAME::TextureLibrary::TextureToUVInfo* info) {
		std::regex regexp("[0-9]");
		std::string input;
		std::smatch result; // 保存匹配结果的容器
		bool Z = true;
		int A = name.size() - 1, B;
		for (size_t i = A; i > 0; --i)
		{
			if (name[i] == Char) {
				if (Z) {
					Z = false;
					A -= i;
					B = i - 1;
					info->Y = TextureConverter<int>(name.substr(i + 1, A));
				}
				else {
					B -= i;
					info->X = TextureConverter<int>(name.substr(i + 1, B));
					return;
				}
			}
			else {
				input = name[i];
				if (!std::regex_search(input, result, regexp)) {
					if (!Z) {
						info->X = info->Y;
						info->Y = 1;
					}
					return;
				}
			}
		}
		if (!Z) {
			info->X = info->Y;
			info->Y = 1;
		}
	}

	TextureLibrary::TextureLibrary(VulKan::Device* device, VulKan::CommandPool* commandPool, VulKan::Sampler* sampler, std::string Path, bool UVbool)
	{
		std::vector<std::string> Texture;
		TOOL::FilePath(Path.c_str(), &Texture, "png", "nullptr", nullptr);

		mTextureLibraryData = new ContinuousMap<std::string, TextureToUVInfo>(Texture.size(), ContinuousMap_New);

		for (auto name : Texture)
		{
			std::cout << name << std::endl;
			int texWidth, texHeight, texChannles;
			stbi_uc* pixels = stbi_load((Path + name + ".png").c_str(), &texWidth, &texHeight, &texChannles, STBI_rgb_alpha);

			if (!pixels) {
				throw std::runtime_error("Error: failed to read image data");
			}
			
			TextureToUVInfo* info = mTextureLibraryData->Get(name);
			info->mTexture = new VulKan::Texture(device, commandPool, sampler, pixels, texWidth, texHeight);
			LeaveOnlyLetters(name, '_', info);
			if (UVbool) {
				float mUVs[] = {
				0.0f,			1.0f / info->Y,
				1.0f / info->X,	1.0f / info->Y,
				1.0f / info->X,	0.0f,
				0.0f,			0.0f,
				};
				info->mUV = VulKan::Buffer::createVertexBuffer(device, 8 * sizeof(float), mUVs);
			}

			stbi_image_free(pixels);
		}
	}

	TextureLibrary::~TextureLibrary() {
		for (auto i : *mTextureLibraryData)
		{
			delete i.mTexture;
			delete i.mUV;
		}
		delete mTextureLibraryData;
	}

}
