import uuid
import os

def gen_id():
    return uuid.uuid4().hex[:24].upper()

proj_id = gen_id()
target_id = gen_id()
sources_build_phase_id = gen_id()
resources_build_phase_id = gen_id()
frameworks_build_phase_id = gen_id()
main_group_id = gen_id()
products_group_id = gen_id()
app_product_id = gen_id()
config_list_proj_id = gen_id()
config_list_target_id = gen_id()
debug_proj_config_id = gen_id()
release_proj_config_id = gen_id()
debug_target_config_id = gen_id()
release_target_config_id = gen_id()

files = []
for root, dirs, filenames in os.walk('DrHD500'):
    for f in filenames:
        if f.endswith('.swift') or f.endswith('.json') or f.endswith('.wav') or f == 'Info.plist':
            path = os.path.join(root, f)
            files.append((f, path, gen_id(), gen_id()))

pbx_file_refs = []
pbx_build_files = []
sources_files = []
resources_files = []

for fname, fpath, file_id, build_id in files:
    if fname == 'Info.plist':
        pbx_file_refs.append(f'\t\t{file_id} /* {fname} */ = {{isa = PBXFileReference; lastKnownFileType = text.plist.xml; path = "{fpath}"; sourceTree = "<group>"; }};')
        continue
    
    if fname.endswith('.swift'):
        ftype = 'sourcecode.swift'
        pbx_file_refs.append(f'\t\t{file_id} /* {fname} */ = {{isa = PBXFileReference; lastKnownFileType = {ftype}; path = "{fpath}"; sourceTree = "<group>"; }};')
        pbx_build_files.append(f'\t\t{build_id} /* {fname} in Sources */ = {{isa = PBXBuildFile; fileRef = {file_id} /* {fname} */; }};')
        sources_files.append(f'\t\t\t\t{build_id} /* {fname} in Sources */,')
    else:
        ftype = 'file'
        pbx_file_refs.append(f'\t\t{file_id} /* {fname} */ = {{isa = PBXFileReference; lastKnownFileType = {ftype}; path = "{fpath}"; sourceTree = "<group>"; }};')
        pbx_build_files.append(f'\t\t{build_id} /* {fname} in Resources */ = {{isa = PBXBuildFile; fileRef = {file_id} /* {fname} */; }};')
        resources_files.append(f'\t\t\t\t{build_id} /* {fname} in Resources */,')

group_children = []
for fname, fpath, file_id, _ in files:
    group_children.append(f'\t\t\t\t{file_id} /* {fname} */,')

content = f"""// !$*UTF8*$!
{{
	archiveVersion = 1;
	classes = {{
	}};
	objectVersion = 56;
	objects = {{

/* Begin PBXBuildFile section */
{os.linesep.join(pbx_build_files)}
/* End PBXBuildFile section */

/* Begin PBXFileReference section */
		{app_product_id} /* DrHD500.app */ = {{isa = PBXFileReference; explicitFileType = wrapper.application; includeInIndex = 0; path = DrHD500.app; sourceTree = BUILT_PRODUCTS_DIR; }};
{os.linesep.join(pbx_file_refs)}
/* End PBXFileReference section */

/* Begin PBXFrameworksBuildPhase section */
		{frameworks_build_phase_id} /* Frameworks */ = {{
			isa = PBXFrameworksBuildPhase;
			buildActionMask = 2147483647;
			files = (
			);
			runOnlyForDeploymentPostprocessing = 0;
		}};
/* End PBXFrameworksBuildPhase section */

/* Begin PBXGroup section */
		{main_group_id} = {{
			isa = PBXGroup;
			children = (
{os.linesep.join(group_children)}
				{products_group_id} /* Products */,
			);
			sourceTree = "<group>";
		}};
		{products_group_id} /* Products */ = {{
			isa = PBXGroup;
			children = (
				{app_product_id} /* DrHD500.app */,
			);
			name = Products;
			sourceTree = "<group>";
		}};
/* End PBXGroup section */

/* Begin PBXNativeTarget section */
		{target_id} /* DrHD500 */ = {{
			isa = PBXNativeTarget;
			buildConfigurationList = {config_list_target_id} /* Build configuration list for PBXNativeTarget "DrHD500" */;
			buildPhases = (
				{sources_build_phase_id} /* Sources */,
				{frameworks_build_phase_id} /* Frameworks */,
				{resources_build_phase_id} /* Resources */,
			);
			buildRules = (
			);
			dependencies = (
			);
			name = DrHD500;
			productName = DrHD500;
			productReference = {app_product_id} /* DrHD500.app */;
			productType = "com.apple.product-type.application";
		}};
/* End PBXNativeTarget section */

/* Begin PBXProject section */
		{proj_id} /* Project object */ = {{
			isa = PBXProject;
			attributes = {{
				BuildIndependentTargetsInParallel = 1;
				LastSwiftUpdateCheck = 1500;
				LastUpgradeCheck = 1500;
				TargetAttributes = {{
					{target_id} = {{
						CreatedOnToolsVersion = 15.0;
					}};
				}};
			}};
			buildConfigurationList = {config_list_proj_id} /* Build configuration list for PBXProject "DrHD500" */;
			compatibilityVersion = "Xcode 14.0";
			developmentRegion = en;
			hasScannedForEncodings = 0;
			knownRegions = (
				en,
				Base,
			);
			mainGroup = {main_group_id};
			productRefGroup = {products_group_id} /* Products */;
			projectDirPath = "";
			projectRoot = "";
			targets = (
				{target_id} /* DrHD500 */,
			);
		}};
/* End PBXProject section */

/* Begin PBXResourcesBuildPhase section */
		{resources_build_phase_id} /* Resources */ = {{
			isa = PBXResourcesBuildPhase;
			buildActionMask = 2147483647;
			files = (
{os.linesep.join(resources_files)}
			);
			runOnlyForDeploymentPostprocessing = 0;
		}};
/* End PBXResourcesBuildPhase section */

/* Begin PBXSourcesBuildPhase section */
		{sources_build_phase_id} /* Sources */ = {{
			isa = PBXSourcesBuildPhase;
			buildActionMask = 2147483647;
			files = (
{os.linesep.join(sources_files)}
			);
			runOnlyForDeploymentPostprocessing = 0;
		}};
/* End PBXSourcesBuildPhase section */

/* Begin XCBuildConfiguration section */
		{debug_proj_config_id} /* Debug */ = {{
			isa = XCBuildConfiguration;
			buildSettings = {{
				ALWAYS_SEARCH_USER_PATHS = NO;
				CLANG_ANALYZER_NONNULL = YES;
				CLANG_ENABLE_MODULES = YES;
				CLANG_ENABLE_OBJC_ARC = YES;
				ENABLE_TESTABILITY = YES;
				GCC_DYNAMIC_NO_PIC = NO;
				GCC_OPTIMIZATION_LEVEL = 0;
				GCC_PREPROCESSOR_DEFINITIONS = (
					"DEBUG=1",
					"$(inherited)",
				);
				IPHONEOS_DEPLOYMENT_TARGET = 16.0;
				SDKROOT = iphoneos;
				SWIFT_ACTIVE_COMPILATION_CONDITIONS = DEBUG;
				SWIFT_OPTIMIZATION_LEVEL = "-Onone";
			}};
			name = Debug;
		}};
		{release_proj_config_id} /* Release */ = {{
			isa = XCBuildConfiguration;
			buildSettings = {{
				ALWAYS_SEARCH_USER_PATHS = NO;
				CLANG_ANALYZER_NONNULL = YES;
				CLANG_ENABLE_MODULES = YES;
				CLANG_ENABLE_OBJC_ARC = YES;
				COPY_PHASE_STRIP = NO;
				ENABLE_NS_ASSERTIONS = NO;
				IPHONEOS_DEPLOYMENT_TARGET = 16.0;
				SDKROOT = iphoneos;
				SWIFT_COMPILATION_MODE = wholemodule;
				SWIFT_OPTIMIZATION_LEVEL = "-O";
			}};
			name = Release;
		}};
		{debug_target_config_id} /* Debug */ = {{
			isa = XCBuildConfiguration;
			buildSettings = {{
				ASSETCATALOG_COMPILER_APPICON_NAME = AppIcon;
				CODE_SIGN_STYLE = Automatic;
				CURRENT_PROJECT_VERSION = 1;
				GENERATE_INFOPLIST_FILE = NO;
				INFOPLIST_FILE = DrHD500/App/Info.plist;
				LD_RUNPATH_SEARCH_PATHS = (
					"$(inherited)",
					"@executable_path/Frameworks",
				);
				MARKETING_VERSION = 1.0;
				PRODUCT_BUNDLE_IDENTIFIER = com.drhd.DrHD500;
				PRODUCT_NAME = "$(TARGET_NAME)";
				SWIFT_EMIT_LOC_STRINGS = YES;
				SWIFT_VERSION = 5.0;
				TARGETED_DEVICE_FAMILY = "1,2";
			}};
			name = Debug;
		}};
		{release_target_config_id} /* Release */ = {{
			isa = XCBuildConfiguration;
			buildSettings = {{
				ASSETCATALOG_COMPILER_APPICON_NAME = AppIcon;
				CODE_SIGN_STYLE = Automatic;
				CURRENT_PROJECT_VERSION = 1;
				GENERATE_INFOPLIST_FILE = NO;
				INFOPLIST_FILE = DrHD500/App/Info.plist;
				LD_RUNPATH_SEARCH_PATHS = (
					"$(inherited)",
					"@executable_path/Frameworks",
				);
				MARKETING_VERSION = 1.0;
				PRODUCT_BUNDLE_IDENTIFIER = com.drhd.DrHD500;
				PRODUCT_NAME = "$(TARGET_NAME)";
				SWIFT_EMIT_LOC_STRINGS = YES;
				SWIFT_VERSION = 5.0;
				TARGETED_DEVICE_FAMILY = "1,2";
			}};
			name = Release;
		}};
/* End XCBuildConfiguration section */

/* Begin XCConfigurationList section */
		{config_list_proj_id} /* Build configuration list for PBXProject "DrHD500" */ = {{
			isa = XCConfigurationList;
			buildConfigurations = (
				{debug_proj_config_id} /* Debug */,
				{release_proj_config_id} /* Release */,
			);
			defaultConfigurationIsVisible = 0;
			defaultConfigurationName = Release;
		}};
		{config_list_target_id} /* Build configuration list for PBXNativeTarget "DrHD500" */ = {{
			isa = XCConfigurationList;
			buildConfigurations = (
				{debug_target_config_id} /* Debug */,
				{release_target_config_id} /* Release */,
			);
			defaultConfigurationIsVisible = 0;
			defaultConfigurationName = Release;
		}};
/* End XCConfigurationList section */

	}};
	rootObject = {proj_id} /* Project object */;
}}
"""

with open('DrHD500.xcodeproj/project.pbxproj', 'w') as f:
    f.write(content)
print('project.pbxproj created cleanly')
