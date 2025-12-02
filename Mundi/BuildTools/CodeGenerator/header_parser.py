"""
C++ 헤더 파일 파싱 모듈
UPROPERTY와 UFUNCTION 매크로를 찾아서 파싱합니다.
"""

import re
from pathlib import Path
from dataclasses import dataclass, field
from typing import List, Dict, Optional, Tuple
from macro_parser import MacroParser, create_macro_parser


class Property:
    """프로퍼티 정보"""
    # MacroParser 인스턴스 (클래스 변수 - dataclass 밖에서 선언)
    _macro_parser: Optional['MacroParser'] = None

    @classmethod
    def set_macro_parser(cls, parser: 'MacroParser'):
        """MacroParser 인스턴스 설정 (전역)"""
        cls._macro_parser = parser

    def __init__(
        self,
        name: str,
        type: str,
        category: str = "",
        editable: bool = True,
        tooltip: str = "",
        min_value: float = 0.0,
        max_value: float = 0.0,
        has_range: bool = False,
        metadata: Optional[Dict[str, str]] = None
    ):
        self.name = name
        self.type = type
        self.category = category
        self.editable = editable
        self.tooltip = tooltip
        self.min_value = min_value
        self.max_value = max_value
        self.has_range = has_range
        self.metadata = metadata or {}

    def _get_property_type_enum(self, type_str: str) -> str:
        """타입 문자열 -> EPropertyType enum 변환"""
        type_lower = type_str.lower().strip()

        # Primitive 타입 먼저 체크
        if type_lower in ['bool']:
            return 'EPropertyType::Bool'
        elif type_lower in ['int32', 'int', 'uint32', 'unsigned int']:
            return 'EPropertyType::Int32'
        elif type_lower in ['float', 'double']:
            return 'EPropertyType::Float'
        elif type_lower in ['fstring']:
            return 'EPropertyType::FString'
        elif type_lower in ['fname']:
            return 'EPropertyType::FName'
        elif type_lower in ['fvector']:
            return 'EPropertyType::FVector'
        elif type_lower in ['fvector2d']:
            return 'EPropertyType::FVector2D'
        elif type_lower in ['flinearcolor']:
            return 'EPropertyType::FLinearColor'
        # UObject 포인터 타입
        elif '*' in type_str:
            if 'umaterial' in type_lower:
                return 'EPropertyType::Material'
            elif 'utexture' in type_lower:
                return 'EPropertyType::Texture'
            elif 'usound' in type_lower:
                return 'EPropertyType::Sound'
            elif 'ustaticmesh' in type_lower:
                return 'EPropertyType::StaticMesh'
            elif 'uskeletalmesh' in type_lower:
                return 'EPropertyType::SkeletalMesh'
            else:
                return 'EPropertyType::ObjectPtr'
        else:
            return 'EPropertyType::ObjectPtr'  # Fallback

    def get_property_type_macro(self) -> str:
        """타입에 맞는 ADD_PROPERTY 매크로 결정 (동적 감지)"""
        type_lower = self.type.lower()

        # TSubclassOf<T> 타입 체크 (가장 먼저 - 가장 구체적)
        if 'tsubclassof' in type_lower:
            template_args = HeaderParser._extract_template_args(self.type)
            if template_args:
                base_class = template_args[0].strip()  # "UAnimInstance"
                # BaseClass를 Metadata에 저장 (PropertyRenderer에서 필터링에 사용)
                self.metadata['BaseClass'] = base_class
            return 'ADD_PROPERTY'

        # TMap 타입 체크 (TArray보다 먼저 - 더 구체적)
        if 'tmap' in type_lower:
            template_args = HeaderParser._extract_template_args(self.type)
            if len(template_args) >= 2:
                # TMap<Key, Value, ...> 에서 Key와 Value만 사용
                key_type = template_args[0]
                value_type = template_args[1]
                self.metadata['key_type'] = self._get_property_type_enum(key_type)
                self.metadata['value_type'] = self._get_property_type_enum(value_type)
            return 'ADD_PROPERTY_MAP'

        # TArray 타입 체크
        if 'tarray' in type_lower:
            template_args = HeaderParser._extract_template_args(self.type)
            if template_args:
                inner_type = template_args[0].strip()
                inner_type_enum = self._get_property_type_enum(inner_type)
                self.metadata['inner_type'] = inner_type_enum
                # Struct 타입인 경우 (F로 시작하는 사용자 정의 구조체)
                if inner_type_enum == 'EPropertyType::Struct' or (
                    inner_type.startswith('F') and
                    inner_type not in ['FString', 'FName', 'FVector', 'FVector2D', 'FLinearColor'] and
                    '*' not in inner_type
                ):
                    self.metadata['struct_type'] = inner_type
                    return 'ADD_PROPERTY_STRUCT_ARRAY'
            return 'ADD_PROPERTY_ARRAY'

        # 특수 타입 체크 (포인터보다 먼저)
        if 'ucurve' in type_lower or 'fcurve' in type_lower:
            # MacroParser에서 CURVE 매크로 찾기
            if self._macro_parser and 'ADD_PROPERTY_CURVE' in self._macro_parser.macros:
                return 'ADD_PROPERTY_CURVE'
            return 'ADD_PROPERTY_CURVE'

        # SRV (Shader Resource View) 타입 체크
        if 'srv' in type_lower or 'shaderresourceview' in type_lower:
            # MacroParser에서 SRV 매크로 찾기
            if self._macro_parser and 'ADD_PROPERTY_SRV' in self._macro_parser.macros:
                return 'ADD_PROPERTY_SRV'
            return 'ADD_PROPERTY_SRV'

        # 포인터 타입 체크 - MacroParser 사용 (자동 감지)
        if '*' in self.type:
            # Fallback: 정확한 타입 매칭 방식
            # 'class ' 접두사와 '*' 접미사 제거 후 정확히 비교
            base_type = type_lower.replace('class ', '').replace('*', '').strip()

            # 리소스 타입들은 정확한 클래스명으로만 매칭
            # (Component, Instance 등 파생 타입 제외)
            if base_type in ['utexture', 'utexture2d']:
                return 'ADD_PROPERTY_TEXTURE'
            elif base_type == 'ustaticmesh':
                return 'ADD_PROPERTY_STATICMESH'
            elif base_type == 'uskeletalmesh':
                return 'ADD_PROPERTY_SKELETALMESH'
            elif base_type == 'umaterial':
                return 'ADD_PROPERTY_MATERIAL'
            elif base_type in ['usound', 'usoundbase']:
                return 'ADD_PROPERTY_AUDIO'
            elif base_type == 'uparticlesystem':
                return 'ADD_PROPERTY_PARTICLESYSTEM'

            if self._macro_parser:
                macro_name = self._macro_parser.get_macro_for_type(self.type)
                if macro_name:
                    return macro_name

        # 스크립트 파일 타입 체크
        # 옵션 1: ScriptFile 메타데이터가 명시된 경우 (우선순위)
        if 'ScriptFile' in self.metadata:
            return 'ADD_PROPERTY_SCRIPT'

        # 옵션 2: 변수명 기반 자동 인식 (FString 타입 + 변수명에 'script' 또는 'lua' 포함)
        if self.type == 'FString':
            name_lower = self.name.lower()
            if 'script' in name_lower or 'lua' in name_lower:
                # 기본 확장자 설정 (.lua)
                if 'ScriptFile' not in self.metadata:
                    self.metadata['ScriptFile'] = '.lua'
                return 'ADD_PROPERTY_SCRIPT'

        # 범위가 있는 프로퍼티
        if self.has_range:
            return 'ADD_PROPERTY_RANGE'

        return 'ADD_PROPERTY'


@dataclass
class Parameter:
    """함수 파라미터 정보"""
    name: str
    type: str


@dataclass
class Function:
    """함수 정보"""
    name: str
    display_name: str
    return_type: str
    parameters: List[Parameter] = field(default_factory=list)
    is_const: bool = False
    metadata: Dict[str, str] = field(default_factory=dict)

    def get_parameter_types_string(self) -> str:
        """템플릿 파라미터 문자열 생성: <UClass, uint32, const FString&, ...>"""
        if not self.parameters:
            return ""
        param_types = ", ".join([p.type for p in self.parameters])
        return f", {param_types}"


@dataclass
class EnumInfo:
    """Enum 정보"""
    name: str
    header_file: Path
    values: Dict[str, int] = field(default_factory=dict)  # {name: value}
    underlying_type: str = "uint8"  # enum class EName : uint8
    display_name: str = ""  # UENUM(DisplayName="...")
    description: str = ""   # UENUM(Description="...")

@dataclass
class ClassInfo:
    """클래스/구조체 정보"""
    name: str
    parent: str  # struct는 빈 문자열 가능
    header_file: Path
    type: str = "class"  # "class" 또는 "struct"
    properties: List[Property] = field(default_factory=list)
    functions: List[Function] = field(default_factory=list)
    is_component: bool = False
    is_spawnable: bool = False
    is_abstract: bool = False  # UCLASS(Abstract) 플래그
    not_spawnable: bool = False  # UCLASS(NotSpawnable) 플래그 - 시스템 액터용
    display_name: str = ""
    description: str = ""
    uclass_metadata: Dict[str, str] = field(default_factory=dict)
    enums: List[EnumInfo] = field(default_factory=list)  # 헤더 내 UENUM들


class HeaderParser:
    """C++ 헤더 파일 파서"""

    # UPROPERTY 시작 패턴 (괄호는 별도 파싱)
    UPROPERTY_START = re.compile(r'UPROPERTY\s*\(')

    def __init__(self, project_root: Optional[Path] = None):
        """
        HeaderParser 초기화

        Args:
            project_root: 프로젝트 루트 경로 (MacroParser 초기화용)
                         None이면 현재 파일 기준으로 자동 감지
        """
        if project_root is None:
            # 현재 파일(header_parser.py)의 상위 2단계가 프로젝트 루트
            project_root = Path(__file__).parent.parent.parent

        # MacroParser 생성 및 파싱
        self.macro_parser = create_macro_parser(project_root)

        # Property 클래스에 MacroParser 설정 (전역)
        Property.set_macro_parser(self.macro_parser)

        print(f"[HeaderParser] Initialized with {len(self.macro_parser.macros)} macros from ObjectMacros.h")

    # UCLASS/USTRUCT 시작 패턴
    UCLASS_START = re.compile(r'UCLASS\s*\(')
    USTRUCT_START = re.compile(r'USTRUCT\s*\(')

    # 기존 패턴들
    UFUNCTION_PATTERN = re.compile(
        r'UFUNCTION\s*\((.*?)\)\s*'
        r'(.*?)\s+(\w+)\s*\((.*?)\)\s*(const)?\s*[;{]',
        re.DOTALL
    )

    CLASS_PATTERN = re.compile(
        r'class\s+(\w+)\s*:\s*public\s+(\w+)'
    )

    # struct 패턴 (상속 선택적)
    STRUCT_PATTERN = re.compile(
        r'struct\s+(\w+)'
    )

    GENERATED_REFLECTION_PATTERN = re.compile(
        r'GENERATED_REFLECTION_BODY\(\)'
    )

    @staticmethod
    def _extract_balanced_parens(text: str, start_pos: int) -> Tuple[str, int]:
        """괄호 매칭하여 내용 추출. Returns (content, end_position)"""
        depth = 1
        i = start_pos
        while i < len(text) and depth > 0:
            if text[i] == '(':
                depth += 1
            elif text[i] == ')':
                depth -= 1
            i += 1
        return text[start_pos:i-1], i

    @staticmethod
    def _extract_template_args(type_str: str) -> List[str]:
        """템플릿 인자 추출 (중첩 템플릿 지원)
        예: "TMap<FString, int32, FDefaultSetAllocator>" -> ["FString", "int32", "FDefaultSetAllocator"]
        예: "TArray<TMap<FString, int32>>" -> ["TMap<FString, int32>"]
        """
        # < > 찾기
        start = type_str.find('<')
        end = type_str.rfind('>')
        if start == -1 or end == -1:
            return []

        content = type_str[start+1:end]
        args = []
        current_arg = ""
        depth = 0

        for ch in content:
            if ch == '<':
                depth += 1
                current_arg += ch
            elif ch == '>':
                depth -= 1
                current_arg += ch
            elif ch == ',' and depth == 0:
                # 최상위 레벨의 콤마만 구분자로 사용
                args.append(current_arg.strip())
                current_arg = ""
            else:
                current_arg += ch

        if current_arg.strip():
            args.append(current_arg.strip())

        return args

    @staticmethod
    def _parse_uproperty_declarations(content: str):
        """UPROPERTY 선언 찾기 (괄호 매칭 지원, 주석 제외)"""
        results = []
        pos = 0

        while True:
            match = HeaderParser.UPROPERTY_START.search(content, pos)
            if not match:
                break

            # 주석 처리된 UPROPERTY인지 확인
            line_start = content.rfind('\n', 0, match.start()) + 1
            line_before_uproperty = content[line_start:match.start()]

            # // 주석이나 /* */ 주석 안에 있는지 확인
            if '//' in line_before_uproperty:
                # 같은 줄에 // 주석이 있으면 건너뛰기
                pos = match.end()
                continue

            # 여러 줄 주석 /* */ 체크
            last_comment_start = content.rfind('/*', 0, match.start())
            last_comment_end = content.rfind('*/', 0, match.start())
            if last_comment_start > last_comment_end:
                # /* 안에 있으면 건너뛰기
                pos = match.end()
                continue

            # 괄호 안 메타데이터 추출
            metadata_start = match.end()
            metadata, metadata_end = HeaderParser._extract_balanced_parens(content, metadata_start)

            # 메타데이터 다음에 타입과 변수명 찾기
            remaining = content[metadata_end:metadata_end+200]  # 적당한 범위만
            # 타입과 변수명을 매칭 (초기화 부분은 무시: =...; 또는 {...}; 또는 ;)
            var_match = re.match(r'\s*([\w<>*:,\s]+?)\s+(\w+)\s*(?:[;=]|{[^}]*};?)', remaining)

            if var_match:
                var_type = var_match.group(1).strip()
                var_name = var_match.group(2)
                results.append((metadata, var_type, var_name))

            pos = metadata_end

        return results

    @staticmethod
    def _remove_comments(content: str) -> str:
        """C++ 주석 제거 (// 및 /* */ 주석)"""
        # 여러 줄 주석 /* */ 제거
        content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
        # 한 줄 주석 // 제거
        content = re.sub(r'//.*?$', '', content, flags=re.MULTILINE)
        return content

    def parse_header(self, header_path: Path) -> Optional[ClassInfo]:
        """헤더 파일 파싱 (UCLASS/USTRUCT/UENUM 모두 지원)"""
        content = header_path.read_text(encoding='utf-8')

        # 주석을 제거한 버전으로 파싱
        content_no_comments = self._remove_comments(content)

        # GENERATED_REFLECTION_BODY() 체크
        has_reflection_body = self.GENERATED_REFLECTION_PATTERN.search(content_no_comments)

        # UENUM() 체크
        has_uenum = re.search(r'UENUM\s*\(', content_no_comments)

        # 둘 다 없으면 무시
        if not has_reflection_body and not has_uenum:
            return None

        # UENUM만 있는 경우: enum만 파싱하여 반환
        if has_uenum and not has_reflection_body:
            # 파일명 기반으로 더미 ClassInfo 생성
            dummy_name = header_path.stem  # "AnimationTypes" 등
            class_info = ClassInfo(
                name=dummy_name,
                parent="",
                header_file=header_path,
                type="enum_only"  # UENUM만 있는 특수 타입
            )
            # UENUM 파싱
            class_info.enums = self._parse_enums(content_no_comments, header_path)
            return class_info

        # UCLASS vs USTRUCT 구분
        uclass_match = self.UCLASS_START.search(content_no_comments)
        ustruct_match = self.USTRUCT_START.search(content_no_comments)

        if uclass_match:
            # UCLASS: class 패턴으로 파싱
            class_match = self.CLASS_PATTERN.search(content_no_comments)
            if not class_match:
                return None

            class_name = class_match.group(1)
            parent_name = class_match.group(2)
            type_kind = "class"
            metadata_match = uclass_match

        elif ustruct_match:
            # USTRUCT: struct 패턴으로 파싱
            struct_match = self.STRUCT_PATTERN.search(content_no_comments)
            if not struct_match:
                return None

            class_name = struct_match.group(1)
            parent_name = ""  # struct는 상속 없음
            type_kind = "struct"
            metadata_match = ustruct_match

        else:
            # UCLASS/USTRUCT 없으면 무시
            return None

        class_info = ClassInfo(
            name=class_name,
            parent=parent_name,
            header_file=header_path,
            type=type_kind
        )

        # 메타데이터 파싱 (UCLASS/USTRUCT 공통)
        if metadata_match:
            metadata_start = metadata_match.end()
            metadata, _ = self._extract_balanced_parens(content_no_comments, metadata_start)
            class_info.uclass_metadata = self._parse_metadata(metadata)

            # DisplayName과 Description 추출
            if 'DisplayName' in class_info.uclass_metadata:
                class_info.display_name = class_info.uclass_metadata['DisplayName']
            if 'Description' in class_info.uclass_metadata:
                class_info.description = class_info.uclass_metadata['Description']

            # Abstract 플래그 확인
            if 'Abstract' in class_info.uclass_metadata:
                class_info.is_abstract = True

            # NotSpawnable 플래그 확인 (시스템 액터용)
            if 'NotSpawnable' in class_info.uclass_metadata:
                class_info.not_spawnable = True

        # UPROPERTY 파싱 (주석 제거된 버전에서)
        uproperty_decls = self._parse_uproperty_declarations(content_no_comments)
        for metadata_str, prop_type, prop_name in uproperty_decls:
            prop = self._parse_property(prop_name, prop_type, metadata_str)
            class_info.properties.append(prop)

        # UFUNCTION 파싱 (주석 제거된 버전에서)
        for match in self.UFUNCTION_PATTERN.finditer(content_no_comments):
            metadata_str = match.group(1)
            return_type = match.group(2).strip()
            func_name = match.group(3)
            params_str = match.group(4)
            is_const = match.group(5) is not None

            func = self._parse_function(
                func_name, return_type, params_str,
                metadata_str, is_const
            )
            class_info.functions.append(func)

        # AActor 클래스에 기본 프로퍼티 추가
        if class_name == 'AActor':
            object_name_prop = Property(
                name='ObjectName',
                type='FName',
                category='[액터]',
                editable=True,
                tooltip='액터의 이름입니다'
            )
            # 맨 앞에 추가
            class_info.properties.insert(0, object_name_prop)

        # UActorComponent 클래스에 기본 프로퍼티 추가
        elif class_name == 'UActorComponent':
            object_name_prop = Property(
                name='ObjectName',
                type='FName',
                category='[컴포넌트]',
                editable=True,
                tooltip='컴포넌트의 이름입니다'
            )
            # 맨 앞에 추가
            class_info.properties.insert(0, object_name_prop)

        # UENUM 파싱
        class_info.enums = self._parse_enums(content_no_comments, header_path)

        return class_info

    def _parse_metadata(self, metadata: str) -> Dict[str, str]:
        """메타데이터 문자열을 파싱하여 딕셔너리로 반환"""
        result = {}

        # Key="Value" "Value2" ... 패턴 찾기
        pattern = re.compile(r'(\w+)\s*=\s*((?:"[^"]*"\s*(?:,\s*)?)+)')
        for match in pattern.finditer(metadata):
            key = match.group(1)
            raw_value_str = match.group(2)
            
            lines = re.findall(r'"([^"]*)"', raw_value_str)
            value = "".join(lines)
            result[key] = value

        # 단독 플래그 찾기 (예: Abstract, Blueprintable 등)
        # Key="Value" 패턴이 아닌 단독 단어를 찾음
        # 콤마나 괄호로 구분된 단어들을 추출
        flags = re.findall(r'\b([A-Z]\w+)\b(?!\s*=)', metadata)
        for flag in flags:
            # 'true' 값으로 저장하여 플래그 존재 여부 표시
            if flag not in result:  # Key="Value"로 이미 파싱된 것은 건너뜀
                result[flag] = 'true'

        return result

    def _parse_enums(self, content: str, header_path: Path) -> List[EnumInfo]:
        """UENUM() 파싱"""
        enums = []

        # UENUM() 다음에 enum class 패턴 찾기
        # 예: UENUM()\n enum class EAnimationMode : uint8 { ... };
        pattern = r'UENUM\s*\([^)]*\)\s*enum\s+class\s+(\w+)\s*:\s*(\w+)\s*\{([^}]+)\}'

        for match in re.finditer(pattern, content, re.DOTALL):
            enum_name = match.group(1)
            underlying_type = match.group(2)
            enum_body = match.group(3)

            # enum 값들 파싱
            values = {}
            current_value = 0

            # 콤마로 구분된 항목들 파싱
            items = enum_body.split(',')
            for item in items:
                item = item.strip()
                if not item or item.startswith('//'):
                    continue

                # 주석 제거
                item = re.sub(r'//.*$', '', item).strip()
                if not item:
                    continue

                # 명시적 값 할당 체크 (예: Value = 10)
                assign_match = re.match(r'(\w+)\s*=\s*(\d+)', item)
                if assign_match:
                    name = assign_match.group(1)
                    value = int(assign_match.group(2))
                    values[name] = value
                    current_value = value + 1
                else:
                    # 자동 인덱싱
                    name_match = re.match(r'(\w+)', item)
                    if name_match:
                        name = name_match.group(1)
                        values[name] = current_value
                        current_value += 1

            enum_info = EnumInfo(
                name=enum_name,
                header_file=header_path,
                values=values,
                underlying_type=underlying_type
            )
            enums.append(enum_info)

        return enums

    def _parse_property(self, name: str, type_str: str, metadata: str) -> Property:
        """프로퍼티 메타데이터 파싱"""
        prop = Property(name=name, type=type_str)

        # Category 추출
        category_match = re.search(r'Category\s*=\s*"([^"]+)"', metadata)
        if category_match:
            prop.category = category_match.group(1)

        # EditAnywhere 체크 (PropertyRenderer 위젯용)
        prop.editable = 'EditAnywhere' in metadata

        # LuaReadWrite 체크 (Lua 바인딩용)
        if 'LuaReadWrite' in metadata:
            prop.metadata['LuaReadWrite'] = 'true'

        # Range 추출
        range_match = re.search(r'Range\s*=\s*"([^"]+)"', metadata)
        if range_match:
            range_str = range_match.group(1)
            min_max = range_str.split(',')
            if len(min_max) == 2:
                prop.has_range = True
                prop.min_value = float(min_max[0].strip())
                prop.max_value = float(min_max[1].strip())

        # Tooltip 추출
        tooltip_match = re.search(r'Tooltip\s*=\s*((?:"[^"]*"\s*(?:,\s*)?)+)', metadata)
        if tooltip_match:
            raw_tooltip_str = tooltip_match.group(1)
            lines = re.findall(r'"([^"]*)"', raw_tooltip_str)
            prop.tooltip = "".join(lines)

        # ScriptFile 메타데이터 추출 (예: ScriptFile=".lua")
        script_file_match = re.search(r'ScriptFile\s*=\s*"([^"]+)"', metadata)
        if script_file_match:
            prop.metadata['ScriptFile'] = script_file_match.group(1)

        return prop

    def _parse_function(
        self, name: str, return_type: str,
        params_str: str, metadata: str, is_const: bool
    ) -> Function:
        """함수 메타데이터 파싱"""
        func = Function(
            name=name,
            display_name=name,
            return_type=return_type,
            is_const=is_const
        )

        # DisplayName 추출
        display_match = re.search(r'DisplayName\s*=\s*((?:"[^"]*"\s*(?:,\s*)?)+)', metadata)
        if display_match:
            raw_display_name_str = display_match.group(1)
            lines = re.findall(r'"([^"]*)"', raw_display_name_str)
            func.display_name = "".join(lines)

        # LuaBind 체크
        func.metadata['lua_bind'] = 'LuaBind' in metadata

        # 파라미터 파싱
        if params_str.strip():
            params = [p.strip() for p in params_str.split(',')]
            for param in params:
                # "const FString& Name" -> type="const FString&", name="Name"
                parts = param.rsplit(None, 1)
                if len(parts) == 2:
                    param_type = parts[0]
                    param_name = parts[1]
                    func.parameters.append(Parameter(name=param_name, type=param_type))

        return func

    def find_reflection_classes(self, source_dir: Path) -> List[ClassInfo]:
        """소스 디렉토리에서 GENERATED_REFLECTION_BODY가 있는 모든 클래스 찾기"""
        classes = []

        for header in source_dir.rglob("*.h"):
            try:
                class_info = self.parse_header(header)
                if class_info:
                    classes.append(class_info)
                    print(f"[OK] Found reflection class: {class_info.name} in {header.name}")
            except Exception as e:
                print(f" Error parsing {header}: {e}")

        # 상속 그래프 구축 (각 클래스가 어떤 베이스 클래스를 상속하는지 추적)
        self._build_inheritance_graph(classes)

        return classes

    def _build_inheritance_graph(self, classes: List[ClassInfo]):
        """
        상속 그래프를 구축하여 각 클래스에 is_derived_from() 메서드 추가

        이를 통해 간접 상속도 판단 가능:
        - UMeshComponent → USceneComponent → UActorComponent
        - is_derived_from('UActorComponent') = True
        """
        # 클래스 이름 -> ClassInfo 매핑
        class_map = {cls.name: cls for cls in classes}

        def is_derived_from(class_info: ClassInfo, base_name: str) -> bool:
            """클래스가 base_name을 (직접/간접) 상속하는지 확인"""
            if class_info.name == base_name:
                return True
            if not class_info.parent:
                return False
            if class_info.parent == base_name:
                return True

            # 부모 클래스가 class_map에 있으면 재귀 검색
            parent_class = class_map.get(class_info.parent)
            if parent_class:
                return is_derived_from(parent_class, base_name)

            # 부모가 리플렉션 클래스가 아니면 (예: UObject)
            # 이름으로만 판단
            return class_info.parent == base_name

        # 각 ClassInfo에 메서드 바인딩
        for cls in classes:
            cls.is_derived_from = lambda base_name, c=cls: is_derived_from(c, base_name)
