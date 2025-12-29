#include "engine.h"
#include "engine_world.cpp"
#include "engine_simulation.cpp"

constexpr f32 pixelsPerMeter = 42.85714f;

internal
void AddSineWaveToBuffer(SoundData& dst, float amplitude, float toneHz) {
	static u64 runninngSampleIndex = 0;
	float sampleInterval = static_cast<float>(dst.nSamplesPerSec) / (2 * PI * toneHz);
	float* fData = reinterpret_cast<float*>(dst.data);
	for (int frame = 0; frame < dst.nSamples; frame++) {
		dst.tSine += 1.0f / sampleInterval;
#if 0
		float value = amplitude * sinf(dst.tSine);
#else
		float value = 0;
#endif
		for (size_t channel = 0; channel < dst.nChannels; channel++) {
			*fData++ = value;
		}
		runninngSampleIndex++;
	}
	if (dst.tSine > 2 * PI * toneHz) {
		// NOTE: sinf() seems to be inaccurate for high tSine values and quantization goes crazy 
		dst.tSine -= 2 * PI * toneHz;
	}
}

u32 randomNumbers[] = {
	1706220, 2,	2998326,	7324911,	977285,		7614172,
	5372439,	3343239,	9405090,	493102,		4704943,
	6369811,	4549511,	8835557,	34415,		7580540,
	3802045,	2436711,	3890579,	9233612,	2624780,
	966753,		3838799,	5613984,	9908402,	669180,
	4827667,	5777205,	8909092,	4086663,	558951,
	8469832,	2622484,	4841411,	7100024,	8009380,
	3897537,	9494762,	8461502,	158606,		373031,
	6105258,	1264398,	4643517,	2334477,	7110983,
	7401396,	5884595,	7692621,	5469942,	3114938,
	1622875,	134163,		1809684,	2682374,	8347007,
	5434274,	8352963,	9902538,	9661554,	6635235,
	4486896,	8047031,	3753931,	2112099,	8765638,
	2862273,	2675091,	3117346,	4633880,	7137900,
	6808198,	3644292,	340496,		9395500,	8403385,
	6693618,	1935731,	9761147,	4929305,	1968121,
	2687637,	3132525,	8617055,	9191371,	1655755,
	2420318,	1785622,	1797218,	372211,		1925208,
	6343986,	2421321,	1297186,	2493968,	680611,
	1640785,	896580,		3578674,	680300,		447966,
	65911312, 85701076, 38585727, 19864207, 93213701,
	86059015, 83597669, 51592897, 55990401, 89737413,
	96077213,  6766136, 93220753, 22312803, 59122790,
	13541217, 83518386, 34501890, 94955229, 72664052,
	95292802, 23600377, 84711500, 17562046,  7812682,
	 7406515, 88902784, 10396633, 60266907,  6609267,
	57261290, 38791846, 89770392, 41679002,  7076266,
	58660325, 65310920, 94790593, 56781263, 56366842,
	11964084, 17136007, 15566743, 58641703, 54747897,
	 4943670, 72951689, 29706898, 47318987, 17956833,
	87644957, 47082189, 41038713, 41402539, 53506644,
	31575543, 64385260, 13777503, 40496419, 70939966,
	 4378744,  5966529, 59268642, 11897941,  4258671,
	31926195, 35171484, 62640584, 56203409, 61932028,
	 9269796, 53552309, 39295941,  6798308, 11466217,
	21577236, 64718216, 71862938, 98550788, 50868496,
	38407559, 87919982, 81309878, 27140820, 64007231,
	33754452, 40054032, 75457785,    14105, 99940750,
	29469255, 76961052, 87835958, 21553116, 81027299,
	96628746, 27618520, 11123725, 70503326, 83606710,
	21202855, 92646147, 52595551, 13898000,  7268679,
	31882857, 57684163, 46940820, 25735244, 26998944,
	11103858, 26728452, 89763717, 63712714, 58273913,
	58335843, 33042632, 53247163,   252101, 78176735,
	47755054,  2454802, 28485736, 35583885, 64411947,
	  917883, 52139391, 83622290, 33609080, 26632184,
	 7713662, 37542642, 85874284, 47967880, 59452507,
	61909512, 88530112, 79925345, 97110556, 18229919,
	66346193, 46988602, 92973476, 87033238, 52363794,
	36314470, 61761098, 43346855, 54433252, 94396165,
	54325281, 83564531, 78912625, 87244471, 24415680,
	81749305, 23878864, 67286274, 89726963, 27097630,
	 3736381, 20942646, 60161867, 47825690, 24203512,
	44084854, 98044769, 51405118, 86678079, 37107811,
	 3650450, 35253031,  5524639, 69411856, 66921195,
	  746536, 78813770, 74530716, 51609330, 43449260,
	51378097,  8424384, 81143776, 87730644, 10563359,
	54481087,  5748276, 43677009, 89571788, 68590357,
	74715151, 96301617, 75104144, 77505518, 55384930,
	11102409, 55742154, 63288108, 14992113, 72793332,
	74657144, 14484877, 85437810,  3546790, 85987131,
	63152507,  2679011, 54522917, 61517469, 43093008,
	42705838, 58391601, 35694288, 92950523, 25509730,
	36468561, 36126920,  9721104, 30761447, 13844353,
	16108568, 90407762, 57967190, 77513989, 66170884,
	44369496, 43450688, 53512803, 24108078, 47339900,
	82133690, 24674588,  1788730, 48418705, 59082627,
	60359242, 67778582,  9272446, 62864806, 58926608,
	 8124795, 32696226,  6361765, 52494600, 63813383,
	39288804, 65221870, 86609973, 26441509, 22517786,
	46667264, 68486232, 74654673, 48336699, 75849594,
	46133641, 79489060, 19183792, 52993722, 73991232,
	85425210, 11399062, 90342375, 39547434, 39934837,
	 5766181, 26185722, 70520658, 46672412, 50726502,
	80794129, 63330205, 33760150, 95957857, 93462498,
	59055196, 48292368, 92075395,  2009728, 21070857,
	36973300, 73606532, 31299639, 33276898,  3894327,
	46161708, 63985966, 51566413, 10533248, 80562212,
	11156083, 76592405, 64716949, 44073290, 33580843,
	36554733, 19419093, 72178180, 36496979, 96959084,
	60517319, 98530892, 49222982, 61991884, 56810353,
	26331994, 89573958,  6814030, 96538024, 55590516,
	88091133, 46703039, 23783663, 41003281, 98097262,
	15077206, 80275461, 53546330, 13525777, 57255887,
	97736292, 27712957, 31814403, 16265523, 65829429,
	72634836,  3273940, 82670697, 21276642, 73056613,
	56651825, 71775164, 10946278, 69494176, 18611652,
	14942939, 18854750, 27672538, 57978202, 23279480,
	73417076, 66142628, 19962009, 58941404,  5351322,
	28280608, 71925009, 48059590, 92637588, 69963926,
	83044481, 67925877, 44538042, 41976699, 75787508,
	 7577096, 72451026, 19380976, 68491366, 40163820,
	38533426, 49793124, 17080786, 23682400, 45097311,
	11378151, 54904925, 68775970, 25788150, 51422659,
	44515617, 66655370, 86489665, 24357357, 34608665,
	13049671, 58016117, 64252591, 49131359,  1464760,
	82536865, 36736451, 61100247, 78612082, 95600632,
	48068649, 29821219,   377086, 62891732, 29782730,
	68282451, 45174164, 86603933, 81549738, 28520122,
	77240357, 60972247, 95437123, 22672086,  2499059,
	32210072, 70361552, 77473167, 71360346, 29202283,
	15457458, 40113128, 86451567, 22494360, 76995551,
	48145281, 88755088, 46195404, 35905921, 92083095,
	18301974, 57302895,  6744110, 71195155, 51816487,
	67368093, 59726110, 22620248, 84572167, 59839704,
	69954196, 66970326, 90030163, 22638388, 33896049,
	86129531, 57285955, 49928351,  9519770, 41196934,
	74322781, 43038946, 99968008, 94627565, 53142875,
	26250058, 32445391, 33498697,  3743232, 34611602,
	34251819, 85046795, 19655067,  4156158, 76994919,
	47461103, 14141197, 72965915,  3321795, 77137944,
	82657187, 80656553, 18102920,  6922489, 70194692,
	51193498, 83735601, 33827630,  8037740, 35226350,
	17444792, 52018764,  8061275, 15010377, 17240785,
	93033892, 71555290,  1939359, 83651683, 10731920,
	36326550, 22949028, 33515265, 56786090, 14244969,
	58047903, 62560678, 76996555, 73666557, 94995194,
	69570409, 60226136, 74190674,  6712045, 28238560,
	43305028, 61538029, 34007941, 89372006, 16977257,
	53095409, 16260278, 20773193, 11098703,  3833925,
	34322771, 56158495, 95959979, 48736265, 58089387,
	66509413, 54028509, 77023221, 92283094, 94345740,
	13968664, 99542478, 58894219, 97113757, 86900698,
	 6004175, 37300061, 22457865, 89478787, 41663014,
	19176881, 19897898, 67942289, 12929485, 54597525,
	76993825,  9321282, 80548537, 86225192, 56632718,
	20106360,  7606126, 82950164, 32071119, 36292807,
	27799632, 64175377, 64808635, 71723458, 78225619,
	86490477, 39098286, 66421847, 11370697, 50896164,
	45413937, 90851987, 57842255, 31574295, 32004263,
	39831840, 56455247,   740686, 44594314, 28514571,
	66779624, 47950983, 46845225, 47744841, 42370554,
	34838038, 36498739,  3778359, 63172513,  6829283,
	81821777, 97588000, 21892602, 58323908, 32434211,
	87100208, 11669698,  9988797, 66286128, 86126326,
	79214398, 66384831, 99689322, 23112145, 23459086,
	39931613, 23128891, 65563887, 95024312, 33559547,
	35028877, 25602237, 90158288, 29912665,  2052726,
	44454996, 87877090, 84390906, 94171636, 97220119,
	91637571, 81121836, 31541452, 37749677, 40181141,
	85887812, 55898381, 48586774, 92440250, 94892355,
	20881296, 68530845, 79671784, 88334110, 47127720,
	94547383, 86107864, 97101994, 29703114, 64886523,
	25672107, 20175547, 94523871, 83498403, 32577893,
	78120313, 11275129, 82111644, 74729990,  3846501,
	20577167, 14956302, 25854314, 45803821, 95731911,
	55092061, 30859511, 56186815, 75541406,  3920875,
	63149487, 32175498, 81838680, 77080000, 69478218,
	68623083, 59479677, 42522665, 38306480,  2506959,
	55291269, 34398989, 20292003, 65738768, 86605869,
	88907246, 82262193,  3830522, 10144184, 16618257,
	55733999, 43804898, 90633186, 13305419, 88717332,
	 4944907, 50844241, 60304802, 51640755, 13381330,
	32951522, 47507081, 43780032, 98911152, 62621427,
	56992105, 76882406, 89039824, 12400190, 13219046,
	66940788, 24552639, 79839748, 31718776, 72743085,
	29209569, 35725295, 30166165, 63547884, 80015370,
	78261720, 55930205, 89070310, 75617706,  7506228,
	68171885, 27942000, 55126833, 74636878, 61991306,
	54285343, 20746055, 31616191, 24250084, 48418856,
	82764179, 46961153,  2860621, 20236965, 25430975,
	38262257, 20515368, 52256493, 78698135, 75104852,
	25232257, 15029199, 85694480, 18717381, 23490628,
	43248476,  8399203, 65930227, 34311135, 60249544,
	86856588, 13290783, 93369893, 44564763, 54563629,
	34798768, 29464250, 22022843, 69009721, 18379973,
	62430049, 66694600, 82116797, 48979844, 42632615,
	13416582, 82961172,    832736, 32022213, 78383038,
	64348212, 58410592, 56842375, 64910846, 39273955,
	86704614, 16617890, 55872315, 84440093, 48634551,
	86000146, 59647234, 70025173, 72997770, 30152109,
	47786213, 16947015, 24797559, 13138964,   739928,
	71362633, 54953028, 31688135,  2887929, 92366627,
	  299268, 82989398, 65597019, 65789880, 37340159,
	98830998, 45367735,  8740250,  3932841, 89124714,
	29538098,  7963084, 89598969, 45794207, 90096203,
	54176890, 25636365, 40288988, 93501803, 21539525,
	99835387, 70159849, 82312226, 31419730, 74806391,
	21751676, 19388910, 37115670, 24581520, 16750632,
	11624067, 43673497, 85324456, 90410683,  3835928,
	93268117, 20544433, 12873899, 99374144, 63415941,
	63697091, 11286014, 61582648, 10264053, 10274033,
	58149718,  8789084, 64456791, 33215892,  7545948,
	86619383, 56869672, 62586621, 40876913,  8119661,
	70699343, 20881276, 28897843, 43682123, 16120480,
	64530062,  3973170, 98125940, 35247944,  3236337,
	90582602, 33951505,  1681940, 89958984,  1923987,
	74315661, 73302856, 63613868, 24018074, 57298552,
	36229355, 50716824, 39206694, 42487261, 39386316,
	34359564, 25039298, 98714743, 10580353, 55207774,
	29095464,   747395, 81382230, 96324281, 90647230,
	31840776, 43001957, 58491857, 78874441, 35998691,
	11576241, 92844370, 75158966,  6359408, 94688839,
	83560182, 10347548, 45928825, 68078144, 66993559,
	10958989, 76904260, 58511851, 24926438, 64971955,
	63779426, 43697058, 79949283, 95426638, 96310464,
	69451883, 69466943, 70301241, 23362758, 85943112,
	85177953, 35299419, 94229263, 86457568, 52227785,
	21025722, 27422287, 89408666, 89921191, 89258355,
	  139888, 66461202, 96790060, 45159982,  5024449,
	82142615, 33865254, 96581476, 38486459, 80404063,
	32550390, 70097301, 31012744, 13441475, 65151553,
	81215078, 53081884, 94221190, 61974668, 75167950,
	 4750646, 76605936, 22608216, 24020111, 86874324,
	97750254, 25194260, 93646589, 86628840, 46454724,
	38732830, 44069760, 91903190,  7205119, 11484845,
	66355201, 43649002, 12453442,  9495457, 78490149,
	39762457, 76676453, 77509487,  5503845, 56635925,
	24711580, 24426961, 29444695, 16238968, 48659218,
	99578152, 22365983, 13425687, 39263849, 18220507,
	56750894, 83981445, 62429189, 44544927, 38779113,
	36036961, 24502003, 30416965,  4756799,  8701118,
	34799113, 35845796, 76369552, 62033203, 21477192,
	97904176, 57395020, 96677925, 95937110, 12717323,
	91658697, 94117208,  6355447, 71990966, 27598266,
	 1350928, 69737448, 55823398, 61999162, 39691745,
	96813640, 20490878, 94871065, 48765488, 45859394,
	  310878, 61631471, 73127138, 62149802, 19626765,
	17732208, 32016047, 77370582,  7212279, 93295417,
	70353981, 49250232, 44471625, 35409903, 21441878
};

inline 
void PushDrawCall(DrawCallGroup& group, LoadedBitmap* bitmap, V3 center, V3 rectSize, f32 R, f32 G, f32 B, f32 A, V2 offset) {
	Assert(group.count < ArrayCount(group.drawCalls));
	DrawCall* call = &group.drawCalls[group.count++];
	call->bitmap = bitmap;
	call->center = center;
	call->rectSize = rectSize;
	call->R = R;
	call->G = G;
	call->B = B;
	call->A = A;
	call->offset = offset;
}

inline
void PushBitmap(DrawCallGroup& group, LoadedBitmap* bitmap, V3 center, f32 A, V2 offset) {
	PushDrawCall(group, bitmap, center, {}, 0, 0, 0, A, offset);
}

inline
void PushRect(DrawCallGroup& group, V3 center, V3 rectSize, f32 R, f32 G, f32 B, f32 A, V2 offset) {
	PushDrawCall(group, 0, center, rectSize, R, G, B, A, offset);
}

internal
void RenderHitPoints(DrawCallGroup& group, Entity& entity, V3 center, V2 offset, f32 distBetween, f32 pointSize) {
	V2 realOffset = (offset + V2{ -scast(f32, entity.hitPoints.count - 1) * scast(f32, distBetween + pointSize) / 2.f , 0.f });
	V3 pointSizeVec = V3{ pointSize, pointSize, 0.f };
	for (u32 hitPointIndex = 0; hitPointIndex < entity.hitPoints.count; hitPointIndex++) {
		V3 rectCenter = center + V3{ realOffset.X, realOffset.Y, 0.f };
		PushRect(group, rectCenter, pointSizeVec, 1.f, 0.f, 0.f, 1.f, {});
		realOffset.X += (distBetween + pointSize);
	}
}

internal 
void RenderRectangle(BitmapData& bitmap, V2 start, V2 end, f32 R, f32 G, f32 B) {
	i32 minX = RoundF32ToI32(start.X);
	i32 maxX = RoundF32ToI32(end.X);
	i32 minY = RoundF32ToI32(start.Y);
	i32 maxY = RoundF32ToI32(end.Y);
	if (minX < 0) {
		minX = 0;
	}
	if (minY < 0) {
		minY = 0;
	}
	if (maxX > scast(i32, bitmap.width)) {
		maxX = scast(i32, bitmap.width);
	}
	if (maxY > scast(i32, bitmap.height)) {
		maxY = scast(i32, bitmap.height);
	}
	u32 color = (scast(u32, 255 * R) << 16) +
				(scast(u32, 255 * G) << 8) +
				(scast(u32, 255 * B) << 0);
	u8* row = ptrcast(u8, bitmap.data) + minY * bitmap.pitch + minX * bitmap.bytesPerPixel;
	for (i32 Y = minY; Y < maxY; Y++) {
		u32* pixel = ptrcast(u32, row);
		for (i32 X = minX; X < maxX; X++) {
			*pixel++ = color;
		}
		row += bitmap.pitch;
	}
}

internal
void RenderRectBorders(DrawCallGroup& group, V3 center, V3 size, f32 thickness) {
	PushRect(group, center - V3{ 0.5f * size.X, 0, 0 },
		V3{ thickness, size.Y, size.Z }, 0.f, 0.f, 1.f, 1.f, {});
	PushRect(group, center + V3{ 0.5f * size.X, 0, 0 },
		V3{ thickness, size.Y, size.Z }, 0.f, 0.f, 1.f, 1.f, {});
	PushRect(group, center - V3{ 0, 0.5f * size.Y, 0 },
		V3{ size.X, thickness, size.Z }, 0.f, 0.f, 1.f, 1.f, {});
	PushRect(group, center + V3{ 0, 0.5f * size.Y, 0 },
		V3{ size.X, thickness, size.Z }, 0.f, 0.f, 1.f, 1.f, {});
}

internal
void RenderBitmap(BitmapData& screenBitmap, LoadedBitmap& loadedBitmap, V2 position) {
	i32 minX = RoundF32ToI32(position.X) - loadedBitmap.alignX;
	i32 maxX = minX + loadedBitmap.width;
	i32 minY = RoundF32ToI32(position.Y) - loadedBitmap.alignY;
	i32 maxY = minY + loadedBitmap.height;
	i32 offsetX = 0;
	i32 offsetY = 0;
	if (minX < 0) {
		offsetX = -minX;
		minX = 0;
	}
	if (minY < 0) {
		offsetY = -minY;
		minY = 0;
	}
	if (maxX > scast(i32, screenBitmap.width)) {
		maxX = scast(i32, screenBitmap.width);
	}
	if (maxY > scast(i32, screenBitmap.height)) {
		maxY = scast(i32, screenBitmap.height);
	}
	u32 loadedBitmapPitch = loadedBitmap.width * loadedBitmap.bytesPerPixel;
	u8* dstRow = ptrcast(u8, screenBitmap.data) + minY * screenBitmap.pitch + minX * screenBitmap.bytesPerPixel;
	u8* srcRow = ptrcast(u8, loadedBitmap.data + (loadedBitmap.height - 1 - offsetY) * (loadedBitmap.width) + offsetX);
	for (i32 Y = minY; Y < maxY; Y++) {
		u32* dstPixel = ptrcast(u32, dstRow);
		u32* srcPixel = ptrcast(u32, srcRow);
		for (i32 X = minX; X < maxX; X++) {
			f32 dR = scast(f32, (*dstPixel >> 16) & 0xFF);
			f32 dG = scast(f32, (*dstPixel >> 8) & 0xFF);
			f32 dB = scast(f32, (*dstPixel >> 0) & 0xFF);

			f32 sA = scast(f32, (*srcPixel >> 24) & 0xFF) / 255.f;
			f32 sR = scast(f32, (*srcPixel >> 16) & 0xFF);
			f32 sG = scast(f32, (*srcPixel >> 8) & 0xFF);
			f32 sB = scast(f32, (*srcPixel >> 0) & 0xFF);

			u8 r = scast(u8, sA * sR + (1 - sA) * dR);
			u8 g = scast(u8, sA * sG + (1 - sA) * dG);
			u8 b = scast(u8, sA * sB + (1 - sA) * dB);

			*dstPixel++ = (r << 16) | (g << 8) | (b << 0);
			srcPixel++;
		}
		dstRow += screenBitmap.pitch;
		srcRow -= loadedBitmapPitch;
	}
}

internal
void RenderGround(ProgramState* state, BitmapData& bitmap) {
	u32 randomMin = 0xFFFFFFFF;
	u32 randomMax = 0;
	for (u32 random = 0; random < ArrayCount(randomNumbers); random++) {
		u32 randomNumber = randomNumbers[random];
		if (randomNumber < randomMin) {
			randomMin = randomNumber;
		}
		if (randomNumber > randomMax) {
			randomMax = randomNumber;
		}
	}
	u32 randomNumberIndex = 0;
	f32 range = f4(randomMax - randomMin);
	f32 halfRange = range / 2.f;
	f32 apperanceRadiusMeters = 2.f * pixelsPerMeter;
	for (u32 bmpIndex = 0; bmpIndex < 200; bmpIndex++) {
		V2 position = V2{
			(randomNumbers[randomNumberIndex++] - randomMin) / f4(randomMax) * 2 * apperanceRadiusMeters - apperanceRadiusMeters,
			(randomNumbers[randomNumberIndex++] - randomMin) / f4(randomMax) * 2 * apperanceRadiusMeters - apperanceRadiusMeters
		};
		bool ground = randomNumbers[randomNumberIndex++] > f4(randomMin) + halfRange;
		LoadedBitmap* bmp = 0;
		if (!ground) {
			u32 grassIndex = randomNumbers[randomNumberIndex++] % ArrayCount(state->grassBmps);
			bmp = state->grassBmps + grassIndex;
		}
		else {
			u32 groundIndex = randomNumbers[randomNumberIndex++] % ArrayCount(state->groundBmps);
			bmp = state->groundBmps + groundIndex;
		}
		position.X += (bitmap.width - bmp->width) / 2.f;
		position.Y += (bitmap.height - bmp->height) / 2.f;
		RenderBitmap(bitmap, *bmp, position);
		if (randomNumberIndex >= ArrayCount(randomNumbers) - 4) {
			randomNumberIndex = 0;
		}
	}
}

internal
LoadedBitmap LoadBmpFile(debug_read_entire_file* debugReadEntireFile, const char* filename) {
#pragma pack(push, 1)
	struct BmpHeader {
		u16 signature; // must be 0x42 = BMP
		u32 fileSize;
		u32 reservedZeros;
		u32 bitmapOffset; // where pixels starts
		u32 headerSize;
		u32 width;
		u32 height;
		u16 planes;
		u16 bitsPerPixel;
		u32 compression;
		u32 imageSize;
		u32 resolutionPixPerMeterX;
		u32 resolutionPixPerMeterY;
		u32 colorsUsed;
		u32 ColorsImportant;
		u32 redMask;
		u32 greenMask;
		u32 blueMask;
		u32 alphaMask;
	};
#pragma pack(pop)

	FileData bmpData = debugReadEntireFile(filename);
	if (!bmpData.content) {
		return {};
	}
	BmpHeader* header = ptrcast(BmpHeader, bmpData.content);
	Assert(header->compression == 3);
	Assert(header->bitsPerPixel == 32);
	u32 redShift = LeastSignificantHighBit(header->redMask).index;
	u32 greenShift = LeastSignificantHighBit(header->greenMask).index;
	u32 blueShift = LeastSignificantHighBit(header->blueMask).index;
	u32 alphaShift = LeastSignificantHighBit(header->alphaMask).index;

	LoadedBitmap result = {};
	result.data = ptrcast(u32, ptrcast(u8, bmpData.content) + header->bitmapOffset);
	result.height = header->height;
	result.width = header->width;
	result.bytesPerPixel = header->bitsPerPixel / 8;

	u32* pixels = result.data;
	for (u32 Y = 0; Y < header->height; Y++) {
		for (u32 X = 0; X < header->width; X++) {
			u32 A = (*pixels >> alphaShift) & 0xFF;
			u32 R = (*pixels >> redShift) & 0xFF;
			u32 G = (*pixels >> greenShift) & 0xFF;
			u32 B = (*pixels >> blueShift) & 0xFF;
			*pixels++ = (A << 24) + (R << 16) + (G << 8) + (B << 0);
		}
	}
	
	return result;
}

CollisionVolumeGroup* MakeGroundedCollisionGroup(ProgramState* state, V3 size) {
	CollisionVolumeGroup* group = ptrcast(CollisionVolumeGroup, PushStructSize(state->world.arena, CollisionVolumeGroup));
	group->volumeCount = 1;
	group->volumes = ptrcast(CollisionVolume, PushArray(state->world.arena, group->volumeCount, CollisionVolume));
	group->volumes[0].size = size;
	group->totalVolume = group->volumes[0];
	return group;
}

inline
void InitializeHitPoints(Entity& entity, u32 nHitPoints, u32 amount, u32 max) {
	for (u32 hitPointIndex = 0; hitPointIndex < nHitPoints; hitPointIndex++) {
		Assert(hitPointIndex < ArrayCount(entity.hitPoints.hitPoints));
		entity.hitPoints.hitPoints[hitPointIndex].amount = amount;
		entity.hitPoints.hitPoints[hitPointIndex].max = max;
	}
	entity.hitPoints.count = nHitPoints;
}

internal
u32 AddGroundedEntity(World& world, EntityStorage& storage, u32 absX, u32 absY, u32 absZ,
	CollisionVolumeGroup* collision) {
	V3 offset = V3{ 0, 0, 0.5f * collision->totalVolume.size.Z };
	storage.entity.worldPos = GetChunkPositionFromWorldPosition(world, absX, absY, absZ, offset);
	storage.entity.collision = collision;
	return AddEntity(world, storage);
}


internal
u32 AddWall(ProgramState* state, World& world, u32 absX, u32 absY, u32 absZ) {
	// TODO: Instead of changing world position into chunk position for all the objects just
	// call AddEntity() which will do it for ourselves
	EntityStorage storage = {};
	SetFlag(storage.entity, EntityFlag_StopsOnCollide);
	storage.entity.type = EntityType_Wall;
	return AddGroundedEntity(world, storage, absX, absY, absZ, state->wallCollision);
}

internal
u32 AddSpace(ProgramState* state, World& world, u32 centerX, u32 centerY, u32 minZ, V3 size) {
	EntityStorage storage = {};
	SetFlag(storage.entity, EntityFlag_Traversable);
	storage.entity.type = EntityType_Space;
	storage.entity.worldPos = GetChunkPositionFromWorldPosition(world, centerX, centerY, minZ, V3{ 0, 0, 0.5f * size.Z });
	storage.entity.collision = MakeGroundedCollisionGroup(state, size);
	return AddEntity(world, storage);
}


internal
u32 AddStairs(ProgramState* state, World& world, u32 absX, u32 absY, u32 absZ) {
	EntityStorage storage = {};
	storage.entity.walkableDim = V3{
		state->stairsCollision->totalVolume.size.X,
		state->stairsCollision->totalVolume.size.Y,
		world.tileSizeInMeters.Z
	};
	SetFlag(storage.entity, EntityFlag_Overlaps);
	storage.entity.type = EntityType_Stairs;
	return AddGroundedEntity(world, storage, absX, absY, absZ, state->stairsCollision);
}

internal
u32 AddFamiliar(ProgramState* state, World& world, u32 absX, u32 absY, u32 absZ) {
	EntityStorage storage = {};
	storage.entity.worldPos = GetChunkPositionFromWorldPosition(world, absX, absY, absZ);
	storage.entity.type = EntityType_Familiar;
	storage.entity.collision = state->familiarCollision;
	SetFlag(storage.entity, EntityFlag_Movable);
	return AddEntity(world, storage);
}

internal
u32 AddSword(ProgramState* state, World& world) {
	EntityStorage storage = {};
	storage.entity.worldPos = NullPosition();
	storage.entity.collision = state->swordCollision;
	storage.entity.type = EntityType_Sword;
	SetFlag(storage.entity, EntityFlag_Movable);
	return AddEntity(world, storage);
}

internal
u32 AddMonster(ProgramState* state, World& world, u32 absX, u32 absY, u32 absZ) {
	EntityStorage storage = {};
	SetFlag(storage.entity, EntityFlag_StopsOnCollide | EntityFlag_Movable);
	storage.entity.type = EntityType_Monster;
	InitializeHitPoints(storage.entity, 3, 1, 1);
	return AddGroundedEntity(world, storage, absX, absY, absZ, state->monsterCollision);
}

internal
u32 InitializePlayer(ProgramState* state) {
	EntityStorage storage = {};
	storage.entity.faceDir = 0;
	storage.entity.type = EntityType_Player;
	SetFlag(storage.entity, EntityFlag_StopsOnCollide | EntityFlag_Movable);
	InitializeHitPoints(storage.entity, 4, 1, 1);
	u32 swIndex = AddSword(state, state->world);
	EntityStorage* swordStorage = GetEntityStorage(state->world, swIndex);
	swordStorage->entity.storageIndex = swIndex;
	storage.entity.sword = &swordStorage->entity;
	Assert(storage.entity.sword);
	u32 index = AddGroundedEntity(state->world, storage, 8, 5, 0, state->playerCollision);
	Assert(index);
	if (!state->cameraEntityIndex) {
		state->cameraEntityIndex = index;
	}
	return index;
}

struct TestWall {
	f32 maxX;
	f32 maxY;
	f32 minY;
	f32 deltaX;
	f32 deltaY;
	V3 normal;
};

bool TestForTMinCollision(f32 maxCornerX, f32 maxCornerY, f32 minCornerY, f32 moveDeltaX,
					  f32 moveDeltaY, f32* tMin) {
	if (moveDeltaX != 0) {
		f32 tEpsilon = 0.001f;
		f32 tCollision = maxCornerX / moveDeltaX;
		if (tCollision >= 0.f && tCollision < *tMin) {
			f32 Y = moveDeltaY * tCollision;
			if (Y >= minCornerY && Y < maxCornerY) {
				*tMin = Maximum(0.f, tCollision - tEpsilon);
				return true;
			}
		}
	}
	return false;
}

bool TestForTMaxCollision(f32 maxCornerX, f32 maxCornerY, f32 minCornerY, f32 moveDeltaX,
	f32 moveDeltaY, f32* tMax) {
	if (moveDeltaX != 0) {
		f32 tEpsilon = 0.001f;
		f32 tCollision = maxCornerX / moveDeltaX;
		if (tCollision >= 0.f && tCollision > *tMax) {
			f32 Y = moveDeltaY * tCollision;
			if (Y >= minCornerY && Y < maxCornerY) {
				*tMax = tCollision - tEpsilon;
				return true;
			}
		}
	}
	return false;
}

inline
u32 GetCollisionHashValue(World& world, u32 hashEntry) {
	Assert(hashEntry);
	// TODO: Better hash function
	u32 hash = hashEntry & (ArrayCount(world.hashCollisions) - 1);
	return hash;
}

internal
void AddCollisionRule(World& world, MemoryArena& arena, u32 firstStorageIndex, u32 secondStorageIndex) {
	// TODO hold multiple indexes inside one block instead of pair per block cause this is memory inefficient
	u32 hash = GetCollisionHashValue(world, firstStorageIndex);
	PairwiseCollision* block = world.hashCollisions[hash];
	PairwiseCollision* newPair = 0;
	if (world.freeCollisionsList) {
		newPair = world.freeCollisionsList;
		world.freeCollisionsList = world.freeCollisionsList->next;
	}
	else {
		newPair = ptrcast(PairwiseCollision, PushStructSize(arena, PairwiseCollision));
	}
	newPair->storageIndex1 = firstStorageIndex;
	newPair->storageIndex2 = secondStorageIndex;
	world.hashCollisions[hash] = newPair;
	newPair->next = block;
}

internal
void ClearCollisionRuleForEntityPair(World& world, u32 firstStorageIndex, u32 secondStorageIndex) {
	u32 hash = GetCollisionHashValue(world, firstStorageIndex);
	PairwiseCollision* firstBlock = world.hashCollisions[hash];
	if (!firstBlock) {
		Assert(!"Tried to clear non existing rule. Invalid code path");
		return;
	}
	PairwiseCollision* previousBlock = 0;
	for (PairwiseCollision* block = firstBlock; block; block = block->next) {
		if (!block) {
			Assert(!"Tried to clear non existing rule. Invalid code path");
			break;
		}
		if (block->storageIndex1 == firstStorageIndex &&
			block->storageIndex2 == secondStorageIndex) {
			if (previousBlock) {
				previousBlock->next = block->next;
			}
			else {
				world.hashCollisions[hash] = block->next;
			}
			block->storageIndex1 = 0;
			block->storageIndex2 = 0;
			block->next = world.freeCollisionsList;
			world.freeCollisionsList = block;
			break;
		}
		previousBlock = block;
	}
}

internal
void ClearCollisionRuleForEntity(World& world, u32 entityStorageIndex) {
	u32 hash = GetCollisionHashValue(world, entityStorageIndex);
	PairwiseCollision* firstBlock = world.hashCollisions[hash];
	if (!firstBlock) {
		return;
	}
	PairwiseCollision* previousBlock = 0;
	PairwiseCollision* block = firstBlock;
	while (block) {
		if (block->storageIndex1 == entityStorageIndex) {
			u32 secondStorageIndex = block->storageIndex2;
			ClearCollisionRuleForEntityPair(world, secondStorageIndex, entityStorageIndex);	
			if (previousBlock) {
				previousBlock->next = block->next;
			}
			else {
				world.hashCollisions[hash] = block->next;
			}
			PairwiseCollision* tmp = block->next;
			block->storageIndex1 = 0;
			block->storageIndex2 = 0;
			block->next = world.freeCollisionsList;
			world.freeCollisionsList = block;
			block = tmp;
		}
		else {
			previousBlock = block;
			block = block->next;
		}
	}
}


internal
bool ShouldCollide(World& world, u32 firstStorageIndex, u32 secondStorageIndex) {
	static_assert((ArrayCount(world.hashCollisions) & (ArrayCount(world.hashCollisions) - 1)) == 0 &&
		"hashValue is ANDed with a mask based with assert that the size of hashCollisions is power of two");
	Assert(firstStorageIndex);
	Assert(secondStorageIndex);
	Assert(firstStorageIndex != secondStorageIndex);

	u32 hash = GetCollisionHashValue(world, firstStorageIndex);
	PairwiseCollision* firstBlock = world.hashCollisions[hash];
	for (PairwiseCollision* block = firstBlock; block; block = block->next) {
		if (!block) {
			return true;
		}
		if (block->storageIndex1 == firstStorageIndex &&
			block->storageIndex2 == secondStorageIndex) {
			return false;
		}
	}
	return true;
}

inline
V3 GetEntityGroundLevel(Entity& entity) {
	V3 result = entity.pos - V3{ 0, 0, 0.5f * entity.collision->totalVolume.size.Z };
	return result;
}

inline
void SetEntityGroundLevel(Entity& entity, f32 newGroundLevel) {
	entity.pos.Z = newGroundLevel + 0.5f * entity.collision->totalVolume.size.Z;
}

internal
void HandleOverlap(World& world, Entity& mover, Entity& obstacle, f32* ground) {
	if (mover.type == EntityType_Player && obstacle.type == EntityType_Stairs) {
		Assert(obstacle.type == EntityType_Stairs);
		V3 normalizedPos = PointRelativeToRect(
			GetRectFromCenterDim(obstacle.pos, obstacle.walkableDim), 
			mover.pos
		);
		f32 stairsLowerPosZ = GetEntityGroundLevel(obstacle).Z;
		f32 stairsUpperPosZ = stairsLowerPosZ + obstacle.walkableDim.Z;
		*ground = stairsLowerPosZ + normalizedPos.Y * obstacle.walkableDim.Z;
		//*ground = Clip(*ground, stairsLowerPosZ, stairsUpperPosZ);
	}
}

internal 
bool EntitiesOverlaps(Entity& entity, Entity& other) {
	for (u32 entityVolumeIndex = 0;
		entityVolumeIndex < entity.collision->volumeCount;
		entityVolumeIndex++) {
		CollisionVolume* entityVolume = entity.collision->volumes + entityVolumeIndex;
		for (u32 obstacleVolumeIndex = 0;
			obstacleVolumeIndex < other.collision->volumeCount;
			obstacleVolumeIndex++) {
			CollisionVolume* obstacleVolume = other.collision->volumes + obstacleVolumeIndex;
			if (EntityOverlapsWithRegion(entity.pos + entityVolume->offsetPos, entityVolume->size,
				GetRectFromCenterDim(other.pos + obstacleVolume->offsetPos, obstacleVolume->size)
			)) {
				return true;
			}
		}
	}
	return false;
}

internal
bool HandleCollision(World& world, Entity& mover, Entity& obstacle) {
	// TODO: Think of better approach of collision handling. This is prototype
	bool stopOnCollide = (
		(IsFlagSet(mover, EntityFlag_StopsOnCollide) && 
		IsFlagSet(obstacle, EntityFlag_StopsOnCollide)) ||
		IsFlagSet(obstacle, EntityFlag_Traversable)
	);

	if (mover.type == EntityType_Familiar && obstacle.type == EntityType_Wall) {
		stopOnCollide = true;
	}
	if (!ShouldCollide(world, mover.storageIndex, obstacle.storageIndex)) {
		return stopOnCollide;
	}
	if (mover.type == EntityType_Sword && obstacle.type == EntityType_Monster) {
		if (obstacle.hitPoints.count > 0) {
			obstacle.hitPoints.count--;
		}
		AddCollisionRule(world, world.arena, mover.storageIndex, obstacle.storageIndex);
		AddCollisionRule(world, world.arena, obstacle.storageIndex, mover.storageIndex);
	}
	if (mover.type == EntityType_Player && obstacle.type == EntityType_Stairs) {
		V3 normMoverPos = PointRelativeToRect(
			GetRectFromCenterDim(obstacle.pos, obstacle.walkableDim), 
			mover.pos
		);
		if (normMoverPos.Z < 0.5f && normMoverPos.Y < 0.1f ||
			normMoverPos.Z >= 0.5f && normMoverPos.Y >= 0.9f) {
			stopOnCollide = false;
		}
		else {
			stopOnCollide = true;
		}
	}
	return stopOnCollide;
}

//void QuickSortHitEntitiesByAscendingT(HitEntity* array, u32 low, u32 high) {
//	if (low >= high || low < 0) {
//		return;
//	}
//
//	u32 pivotIndex = low;
//	HitEntity pivot = array[high];
//	for (u32 index = low; index < high - 1; index++) {
//		if (array[index].t <= pivot.t) {
//			HitEntity tmp = array[index];
//			array[index] = array[pivotIndex];
//			array[pivotIndex] = array[index];
//			pivotIndex = low + 1;
//		}
//	}
//	HitEntity tmp = array[pivotIndex];
//	array[pivotIndex] = array[high];
//	array[high] = array[pivotIndex];
//
//	QuickSortHitEntitiesByAscendingT(array, low, pivotIndex - 1);
//	QuickSortHitEntitiesByAscendingT(array, pivotIndex + 1, high);
//}

internal
void MoveEntity(SimRegion& simRegion, ProgramState* state, World& world, Entity& entity, V3 acceleration, f32 dt) {
	f32 distanceRemaining = (entity.distanceRemaining != 0.f) ?
		entity.distanceRemaining :
		100000.f;
	V3 moveDelta = 0.5f * acceleration * Squared(dt) + entity.vel * dt;
	f32 moveDeltaLength = Length(moveDelta);
	if (moveDeltaLength > distanceRemaining) {
		moveDelta *= distanceRemaining / moveDeltaLength;
	}

	entity.vel += acceleration * dt;
	V3 nextPlayerPosition = entity.pos + moveDelta;

	// TODO: G.J.K algorithm for other collision shapes like circles, elipses etc.
	for (u32 iteration = 0; iteration < 4; iteration++) {
		f32 tMin = 1.0f;
		f32 tMax = 0.0f;
		V3 wallNormalMin = {};
		V3 wallNormalMax = {};
		Entity* hitEntityMin = 0;
		Entity* hitEntityMax = 0;
		V3 desiredPosition = entity.pos + moveDelta;
		for (u32 entityIndex = 0; entityIndex < simRegion.entityCount; entityIndex++) {
			Entity* other = simRegion.entities + entityIndex;
			if (!other || IsFlagSet(*other, EntityFlag_NonSpatial) || other->storageIndex == entity.storageIndex) {
				continue;
			}
			if (IsFlagSet(*other, EntityFlag_Traversable)) {
				if (!EntitiesOverlaps(entity, *other)) {
					continue;
				}
				for (u32 entityVolumeIndex = 0; entityVolumeIndex < entity.collision->volumeCount; entityVolumeIndex++) {
					CollisionVolume* entityVolume = entity.collision->volumes + entityVolumeIndex;
					for (u32 obstacleVolumeIndex = 0; obstacleVolumeIndex < other->collision->volumeCount; obstacleVolumeIndex++) {
						CollisionVolume* obstacleVolume = other->collision->volumes + obstacleVolumeIndex;

						V3 diff = other->pos + obstacleVolume->offsetPos - entity.pos - entityVolume->offsetPos;
						V3 minCorner = diff - 0.5f * obstacleVolume->size - 0.5f * entityVolume->size;
						V3 maxCorner = diff + 0.5f * obstacleVolume->size + 0.5f * entityVolume->size;

						// TODO: Handle Z axis in collisions more properly
						if (entity.pos.Z + entityVolume->offsetPos.Z >= maxCorner.Z ||
							entity.pos.Z + entityVolume->offsetPos.Z < minCorner.Z) {
							continue;
						}
						TestWall testWalls[] = {
							TestWall{ maxCorner.X, maxCorner.Y, minCorner.Y, moveDelta.X, moveDelta.Y, V3{ -1.f, 0.f, 0.f } },
							TestWall{ minCorner.X, maxCorner.Y, minCorner.Y, moveDelta.X, moveDelta.Y, V3{ 1.f, 0.f, 0.f } },
							TestWall{ maxCorner.Y, maxCorner.X, minCorner.X, moveDelta.Y, moveDelta.X, V3{ 0.f, -1.f, 0.f } },
							TestWall{ minCorner.Y, maxCorner.X, minCorner.X, moveDelta.Y, moveDelta.X, V3{ 0.f, 1.f, 0.f }}
						};
						// TODO: When hit test is true for specific entity pair: 
						// we can break from volume/volume O^2 loop as a optimization
						if (entity.type == EntityType_Player) {
							int breakHere = 5;
						}
						for (u32 wallIndex = 0; wallIndex < ArrayCount(testWalls); wallIndex++) {
							TestWall* wall = testWalls + wallIndex;
							if (TestForTMaxCollision(wall->maxX, wall->maxY, wall->minY, wall->deltaX,
								wall->deltaY, &tMax)) {
								// Right wall
								wallNormalMax = wall->normal;
								hitEntityMax = other;
							}
						}
					}
				}
			}
			else {
				for (u32 entityVolumeIndex = 0; entityVolumeIndex < entity.collision->volumeCount; entityVolumeIndex++) {
					CollisionVolume* entityVolume = entity.collision->volumes + entityVolumeIndex;
					for (u32 obstacleVolumeIndex = 0; obstacleVolumeIndex < other->collision->volumeCount; obstacleVolumeIndex++) {
						CollisionVolume* obstacleVolume = other->collision->volumes + obstacleVolumeIndex;

						V3 diff = other->pos + obstacleVolume->offsetPos - entity.pos - entityVolume->offsetPos;
						V3 minCorner = diff - 0.5f * obstacleVolume->size - 0.5f * entityVolume->size;
						V3 maxCorner = diff + 0.5f * obstacleVolume->size + 0.5f * entityVolume->size;

						// TODO: Handle Z axis in collisions more properly
						if (entity.pos.Z + entityVolume->offsetPos.Z >= maxCorner.Z ||
							entity.pos.Z + entityVolume->offsetPos.Z < minCorner.Z) {
							continue;
						}
						TestWall testWalls[] = {
							TestWall{ maxCorner.X, maxCorner.Y, minCorner.Y, moveDelta.X, moveDelta.Y, V3{ 1.f, 0.f, 0.f } },
							TestWall{ minCorner.X, maxCorner.Y, minCorner.Y, moveDelta.X, moveDelta.Y, V3{ -1.f, 0.f, 0.f } },
							TestWall{ maxCorner.Y, maxCorner.X, minCorner.X, moveDelta.Y, moveDelta.X, V3{ 0.f, 1.f, 0.f } },
							TestWall{ minCorner.Y, maxCorner.X, minCorner.X, moveDelta.Y, moveDelta.X, V3{ 0.f, -1.f, 0.f }}
						};
						// TODO: When hit test is true for specific entity pair: 
						// we can break from volume/volume O^2 loop as a optimization
						for (u32 wallIndex = 0; wallIndex < ArrayCount(testWalls); wallIndex++) {
							TestWall* wall = testWalls + wallIndex;
							if (TestForTMinCollision(wall->maxX, wall->maxY, wall->minY, wall->deltaX,
								wall->deltaY, &tMin)) {
								// Right wall
								wallNormalMin = wall->normal;
								hitEntityMin = other;
							}
						}
					}
				}
			}
		}
		if (entity.type == EntityType_Player) {
			int breakHere = 5;
		}
		f32 tMove;
		Entity* hitEntity;
		V3 wallNormal;
		if (tMin < tMax) {
			tMove = tMin;
			hitEntity = hitEntityMin;
			wallNormal = wallNormalMin;
		}
		else {
			tMove = tMax;
			hitEntity = hitEntityMax;
			wallNormal = wallNormalMax;
		}		
		entity.pos += moveDelta * tMove;
		if (entity.distanceRemaining != 0.f) {
			constexpr f32 dEpsilon = 0.01f;
			entity.distanceRemaining -= Length(moveDelta * tMove);
			if (entity.distanceRemaining < dEpsilon) {
				entity.distanceRemaining = 0.f;
			}
		}
		if (hitEntity) {
			bool stopOnCollision = HandleCollision(world, entity, *hitEntity);
			if (stopOnCollision) {
				entity.vel -= Inner(entity.vel, wallNormal) * wallNormal;
				moveDelta = desiredPosition - entity.pos;
				moveDelta -= Inner(moveDelta, wallNormal) * wallNormal;
			}
			else {
				// TODO: Can it be done in a better, smarter way?
				// TODO: distance should be updated here as well
				moveDelta = desiredPosition - entity.pos;
				entity.pos += moveDelta * (1.f - tMove);
				break;
			}
		}
		if (tMove == 1.0f) {
			break;
		}
	}

	u32 overlapEntityCount = 0;
	u32 overlapEntities[16] = {};
	// Note: Even if simulation won't be as accurate, because between hit iterations
	// entity could potentially overlap with another entity, having this after all
	// hit iterations seems to be good aproximation cause we don't need to mess up
	// with lots of stuff. It's worth noting that in case of large dt it can result
	// in missing some overlaps 
	// TODO: (maybe it should be done more precisely for simulation with larger dt)
	for (u32 entityIndex = 0; entityIndex < simRegion.entityCount; entityIndex++) {
		Entity* other = simRegion.entities + entityIndex;
		if (!other || IsFlagSet(*other, EntityFlag_NonSpatial) ||
			other->storageIndex == entity.storageIndex) {
			continue;
		}
		if (EntitiesOverlaps(entity, *other)) {
			Assert(overlapEntityCount < ArrayCount(overlapEntities));
			if (overlapEntityCount < ArrayCount(overlapEntities)) {
				overlapEntities[overlapEntityCount++] = entityIndex;
			}
		}
	}

	f32 ground = simRegion.distanceToClosestGroundZ;
	for (u32 overlapEntityIdx = 0; overlapEntityIdx < overlapEntityCount; overlapEntityIdx++) {
		Entity* other = simRegion.entities + overlapEntities[overlapEntityIdx];
		HandleOverlap(world, entity, *other, &ground);
	}
	f32 entityGroundLevel = GetEntityGroundLevel(entity).Z;
	if (entityGroundLevel != ground) {
		if (entity.type == EntityType_Player) {
			int breakHere = 5;
		}
		SetEntityGroundLevel(entity, ground);
		entity.vel.Z = 0.f;
	}

	WorldPosition newEntityPos = OffsetWorldPosition(world, state->cameraPos, entity.pos);
	// TODO: change location of ChangeEntityChunkLocation, it can be potentially problem in the future
	// cause this changes location of low entity which is not updated by EndSimulation(), so it looks
	// like it has been moved by until EndSimulation() is not called its data is not in sync with chunk
	// location
	ChangeEntityChunkLocation(world, world.arena, entity.storageIndex, entity, &entity.worldPos, newEntityPos);
}

internal
void SetCamera(ProgramState* state) {
	EntityStorage* cameraEntityStorage = GetEntityStorage(state->world, state->cameraEntityIndex);
	if (cameraEntityStorage) {
		state->cameraPos = OffsetWorldPosition(state->world, state->cameraPos, cameraEntityStorage->entity.pos);
	}
}

internal
void MakeEntitySpatial(SimRegion& simRegion, World& world, u32 storageEntityIndex, Entity& entity, WorldPosition& newPos) {
	Assert(IsFlagSet(entity, EntityFlag_NonSpatial));
	ChangeEntityChunkLocation(world, world.arena, storageEntityIndex, entity, 0, newPos);
	TryAddEntityToSim(simRegion, world, storageEntityIndex, entity);
	ClearFlag(entity, EntityFlag_NonSpatial);
}

internal
void MakeEntityNonSpatial(ProgramState* state, u32 storageEntityIndex, Entity& entity) {
	Assert(!IsFlagSet(entity, EntityFlag_NonSpatial));
	WorldPosition nullPosition = NullPosition();
	ChangeEntityChunkLocation(state->world, state->world.arena, storageEntityIndex, entity, &entity.worldPos, nullPosition);
	SetFlag(entity, EntityFlag_NonSpatial);
}

internal
V2 MapScreenSpacePosIntoCameraSpace(f32 screenPosX, f32 screenPosY, u32 screenWidth, u32 screenHeight) {
	V2 pos = {};
	pos.X = screenWidth * (screenPosX - 0.5f) / pixelsPerMeter;
	pos.Y = screenHeight * (0.5f - screenPosY) / pixelsPerMeter;
	return pos;
}


extern "C" GAME_MAIN_LOOP_FRAME(GameMainLoopFrame) {
	ProgramState* state = ptrcast(ProgramState, memory.permanentMemory);
	World& world = state->world;
	if (!state->isInitialized) {
		InitializeWorld(world);
		world.arena.data = ptrcast(u8, memory.permanentMemory) + sizeof(ProgramState);
		world.arena.capacity = memory.permanentMemorySize - sizeof(ProgramState);
		world.arena.used = 0;

		state->wallCollision = MakeGroundedCollisionGroup(state, world.tileSizeInMeters);
		state->playerCollision = MakeGroundedCollisionGroup(state, V3{1.0f, 0.55f, 0.25f});
		state->monsterCollision = MakeGroundedCollisionGroup(state, V3{ 1.0f, 1.25f, 0.25f });
		state->familiarCollision = MakeGroundedCollisionGroup(state, V3{ 1.0f, 1.25f, 0.25f });
		state->swordCollision = MakeGroundedCollisionGroup(state, V3{ 0.5f, 0.5f, 0.25f });
		state->stairsCollision = MakeGroundedCollisionGroup(
			state,
			V3{ world.tileSizeInMeters.X, 
				2.0f * world.tileSizeInMeters.Y, 
				1.1f * world.tileSizeInMeters.Z }
		);

		state->highFreqBoundDim = 30.f;
		state->highFreqBoundHeight = 1.2f;
		state->cameraPos = GetChunkPositionFromWorldPosition(
			world, world.tileCountX / 2, world.tileCountY / 2, 0
		);

		state->groundBmps[0] = LoadBmpFile(memory.debugReadEntireFile, "test/ground0.bmp");
		state->groundBmps[1] = LoadBmpFile(memory.debugReadEntireFile, "test/ground1.bmp");
		state->grassBmps[0] = LoadBmpFile(memory.debugReadEntireFile, "test/grass0.bmp");
		state->grassBmps[1] = LoadBmpFile(memory.debugReadEntireFile, "test/grass1.bmp");

		state->playerMoveAnim[0] = LoadBmpFile(memory.debugReadEntireFile, "test/hero-right.bmp");
		state->playerMoveAnim[0].alignX = 0;
		state->playerMoveAnim[0].alignY = 55;

		state->playerMoveAnim[1] = LoadBmpFile(memory.debugReadEntireFile, "test/hero-left.bmp");
		state->playerMoveAnim[1].alignX = 0;
		state->playerMoveAnim[1].alignY = 55;

		state->playerMoveAnim[2] = LoadBmpFile(memory.debugReadEntireFile, "test/hero-up.bmp");
		state->playerMoveAnim[2].alignX = 0;
		state->playerMoveAnim[2].alignY = 55;

		state->playerMoveAnim[3] = LoadBmpFile(memory.debugReadEntireFile, "test/hero-down.bmp");
		state->playerMoveAnim[3].alignX = 0;
		state->playerMoveAnim[3].alignY = 55;

		//TODO pixelsPerMeters as world property?

		bool doorLeft = false;
		bool doorRight = false;
		bool doorUp = false;
		bool doorDown = false;
		bool lvlJustChanged = false;
		bool ladder = false;
		u32 screenX = 0;
		u32 screenY = 0;
		u32 randomNIdx = 0;
		u32 absTileZ = 0;
		for (u32 screenIndex = 0; screenIndex < 100; screenIndex++) {
			randomNIdx = (randomNIdx + 1) % ArrayCount(randomNumbers);
			u32 randomNumber = randomNumbers[randomNIdx];
#if 1
			u32 mod = randomNumber % 3;
#else
			u32 mod = randomNumber % 2;
#endif
			if (ladder) {
				mod = randomNumber % 2;
				ladder = false;
			}
			if (mod == 0) {
				doorRight = true;
			}
			else if (mod == 1) {
				doorUp = true;
			}
			else if (mod == 2) {
				ladder = true;
			}

			u32 roomCenterX = screenX * world.tileCountX + world.tileCountX / 2;
			u32 roomCenterY = screenY * world.tileCountY + world.tileCountY / 2;
			V3 roomSize = V3{ world.tileCountX * world.tileSizeInMeters.X,
							  world.tileCountY * world.tileSizeInMeters.Y,
							  world.tileSizeInMeters.Z };
			AddSpace(state, world, roomCenterX, roomCenterY, absTileZ, roomSize);
			for (u32 tileY = 0; tileY < world.tileCountY; tileY++) {
				for (u32 tileX = 0; tileX < world.tileCountX; tileX++) {
					u32 absTileX = screenX * world.tileCountX + tileX;
					u32 absTileY = screenY * world.tileCountY + tileY;

					u32 tileValue = 1;
					bool putStairs = false;
					if (tileX == 0) {
						tileValue = 2;
						if (doorLeft && tileY == world.tileCountY / 2) {
							tileValue = 1;
						}
					} 
					else if (tileY == 0) {
						tileValue = 2;
						if (doorDown && tileX == world.tileCountX / 2) {
							tileValue = 1;
						}
					} 
					else if (tileX == world.tileCountX - 1) {
						tileValue = 2;
						if (doorRight && tileY == world.tileCountY / 2) {
							tileValue = 1;
						}
					}
					else if (tileY == world.tileCountY - 1) {
						tileValue = 2;
						if (doorUp && tileX == world.tileCountX / 2) {
							tileValue = 1;
						}
					}
					u32 stairPosX = 9;
					u32 stairPosY = 5;
					if (ladder && absTileZ == 0 && tileX == stairPosX && tileY == stairPosY) {
						putStairs = true; // Ladder up
					}
					else if (ladder && absTileZ == 1 && tileX == 2 && tileY == 2) {
						//tileValue = 4; // Ladder down
					}
					else if (lvlJustChanged && absTileZ == 0 && tileX == stairPosX && tileY == stairPosY) {
						putStairs = true; // Ladder up
					}
					else if (lvlJustChanged && absTileZ == 1 && tileX == 2 && tileY == 2) {
						//tileValue = 4; // Ladder down
					}
					// TODO: Chunk allocation on demand
					if (tileValue == 2) {
						AddWall(state, world, absTileX, absTileY, absTileZ);
					}
					if (putStairs) {
						AddStairs(state, world, absTileX, absTileY, absTileZ);
					}
					
				}
			}
			doorLeft = doorRight;
			doorDown = doorUp;
			doorUp = false;
			doorRight = false;

			if (mod == 0) {
				screenX++;
			}
			else if (mod == 1) {
				screenY++;
			}
			if (ladder) {
				lvlJustChanged = true;
				absTileZ = scast(u32, !absTileZ);
			}
			else {
				lvlJustChanged = false;
			}
		}
		AddFamiliar(state, world, 17 / 2, 9 / 2, 0);
		AddMonster(state, world, 17 / 2, 7, 0);

		state->isInitialized = true;
	}
	SetCamera(state);
	MemoryArena simArena = {};
	simArena.data = ptrcast(u8, memory.transientMemory);
	simArena.capacity = memory.transientMemorySize;
	V3 cameraBoundsDims = {
		state->highFreqBoundDim * state->world.tileSizeInMeters.X,
		state->highFreqBoundDim * state->world.tileSizeInMeters.Y,
		state->highFreqBoundHeight * state->world.tileSizeInMeters.Z
	};
	Rect3 cameraBounds = GetRectFromCenterDim(V3{ 0, 0, 0 }, cameraBoundsDims);
	SimRegion* simRegion = BeginSimulation(simArena, world, state->cameraPos, cameraBounds);
	for (u32 playerIdx = 0; playerIdx < MAX_CONTROLLERS; playerIdx++) {
		Controller& controller = input.controllers[playerIdx];
		PlayerControls& playerControls = state->playerControls[playerIdx];
		u32 playerLowEntityIndex = state->playerEntityIndexes[playerIdx];
		if (controller.isSpaceDown && playerLowEntityIndex == 0) {
			playerLowEntityIndex = InitializePlayer(state);
			state->playerEntityIndexes[playerIdx] = playerLowEntityIndex;
		}

		Entity* entity = GetEntityByStorageIndex(*simRegion, playerLowEntityIndex);
		if (!entity) {
			continue;
		}
		playerControls.acceleration = {};
		f32 speed = 75.0f;
		if (controller.isADown) {
			entity->faceDir = 1;
			playerControls.acceleration.X -= 1.f;
		}
		if (controller.isWDown) {
			entity->faceDir = 2;
			playerControls.acceleration.Y += 1.f;
		}
		if (controller.isSDown) {
			entity->faceDir = 3;
			playerControls.acceleration.Y -= 1.f;
		}
		if (controller.isDDown) {
			entity->faceDir = 0;
			playerControls.acceleration.X += 1.f;
		}
		if (controller.isSpaceDown) {
			speed = 250.0f;
		}
		if (controller.isMouseLDown && IsFlagSet(*entity->sword, EntityFlag_NonSpatial)) {
			entity->sword->distanceRemaining = 5.f;
			V2 mousePos = MapScreenSpacePosIntoCameraSpace(controller.mouseX, controller.mouseY, bitmap.width, bitmap.height);
			f32 mouseVecLength = Length(mousePos);
			f32 projectileSpeed = 5.f;
			if (mouseVecLength != 0.f) {
				entity->sword->vel.XY = mousePos / Length(mousePos) * projectileSpeed;
			}
			else {
				entity->sword->vel.XY = V2{ projectileSpeed, 0.f };
			}
			
			MakeEntitySpatial(*simRegion, state->world, entity->sword->storageIndex, *entity->sword, entity->worldPos);
		}
		playerControls.acceleration.Z = 0.f;
		if (controller.isUpDown) {
			playerControls.acceleration.Z += 10.0f;
		}
		if (controller.isDownDown) {
			playerControls.acceleration.Z -= 10.0f;
		}
		f32 playerAccLength = Length(playerControls.acceleration);
		if (playerAccLength != 0) {
			playerControls.acceleration *= speed / Length(playerControls.acceleration);
		}
		playerControls.acceleration -= 10.0f * entity->vel;
	}

	RenderRectangle(bitmap, V2{ 0, 0 }, V2{ scast(f32, bitmap.width), scast(f32, bitmap.height) }, 0.5f, 0.5f, 0.5f);
	RenderGround(state, bitmap);
	for (u32 entityIndex = 0; entityIndex < simRegion->entityCount; entityIndex++) {
		Entity* entity = simRegion->entities + entityIndex;
		if (!entity) {
			continue;
		}
		DrawCallGroup drawCalls = {};
		V3 acceleration = V3{ 0, 0, 0 };

		switch(entity->type) {
		case EntityType_Player: {
			PlayerControls* playerControls = 0;
			for (u32 controllerIndex = 0; controllerIndex < MAX_CONTROLLERS; controllerIndex++) {
				u32 index = state->playerEntityIndexes[controllerIndex];
				if (index == entity->storageIndex) {
					playerControls = &state->playerControls[controllerIndex];
					break;
				}
			}
			Assert(playerControls);
			acceleration = playerControls->acceleration;
			PushRect(drawCalls, entity->pos, entity->collision->totalVolume.size, 0.f, 1.f, 1.f, 1.f, {});
			PushBitmap(drawCalls, &state->playerMoveAnim[entity->faceDir], entity->pos, 1.f, 
				entity->collision->totalVolume.size.XY / 2.f);
			RenderHitPoints(drawCalls, *entity, entity->pos, V2{0.f, -0.6f}, 0.1f, 0.2f);
		} break;
		case EntityType_Wall: {
			PushRect(drawCalls, entity->pos, entity->collision->totalVolume.size, 1.f, 1.f, 1.f, 1.f, {});
		} break;
		case EntityType_Stairs: {
			PushRect(drawCalls, entity->pos, entity->collision->totalVolume.size, 0.2f, 0.2f, 0.2f, 1.f, {});
			PushRect(drawCalls, entity->pos + V3{ 0, 0, entity->collision->totalVolume.size.Z }, 
				entity->collision->totalVolume.size, 0.f, 0.f, 0.f, 1.f, {});
		} break;
		case EntityType_Familiar: {
			f32 minDistance = Squared(10.f);
			V3 minDistanceEntityPos = {};
			for (u32 otherEntityIndex = 0; otherEntityIndex < simRegion->entityCount; otherEntityIndex++) {
				Entity* other = simRegion->entities + otherEntityIndex;
				if (other->type == EntityType_Player) {
					f32 distance = LengthSq(other->pos - entity->pos);
					if (distance < minDistance) {
						minDistance = distance;
						minDistanceEntityPos = other->pos;
					}
				}
			}
			f32 speed = 50.0f;
			static float t = 0.f;
			if (minDistance > Squared(2.0f)) {
				acceleration = speed * (minDistanceEntityPos - entity->pos) / SquareRoot(minDistance);
			}
			acceleration.Z = 10.0f * sinf(6 * t);
			t += input.dtFrame;
			acceleration -= 10.0f * entity->vel;
			PushRect(drawCalls, entity->pos, entity->collision->totalVolume.size, 0.f, 0.f, 1.f, 1.f, {});
		} break;
		case EntityType_Monster: {
			PushRect(drawCalls, entity->pos, entity->collision->totalVolume.size, 1.f, 0.5f, 0.f, 1.f, {});
			RenderHitPoints(drawCalls, *entity, entity->pos, V2{ 0.f, -0.9f }, 0.1f, 0.2f);
		} break;
		case EntityType_Sword: {
			PushRect(drawCalls, entity->pos, entity->collision->totalVolume.size, 0.f, 0.f, 0.f, 1.f, {});
			if (entity->distanceRemaining <= 0.f) {
				ClearCollisionRuleForEntity(state->world, entity->storageIndex);
				MakeEntityNonSpatial(state, entity->storageIndex, *entity);
			}
		} break;
		case EntityType_Space: {
			RenderRectBorders(drawCalls, entity->pos, entity->collision->totalVolume.size, 0.2f);
		} break;
		default: Assert(!"Function to draw entity not found!");
		}
		if (IsFlagSet(*entity, EntityFlag_Movable) && !IsFlagSet(*entity, EntityFlag_NonSpatial)) {
			MoveEntity(*simRegion, state, world, *entity, acceleration, input.dtFrame);
		}

		for (u32 drawCallIndex = 0; drawCallIndex < drawCalls.count; drawCallIndex++) {
			DrawCall* call = drawCalls.drawCalls + drawCallIndex;
			V3 groundLevel = call->center - 0.5f * V3{ 0, 0, call->rectSize.Z };
			f32 zFudge = 0.1f * groundLevel.Z;
			V2 center = { (1.f + zFudge) * groundLevel.X * pixelsPerMeter + bitmap.width / 2.0f,
						  scast(f32, bitmap.height) - (1.f + zFudge) * groundLevel.Y * pixelsPerMeter - bitmap.height / 2.0f - groundLevel.Z * pixelsPerMeter };
			V2 size = { (1.f + zFudge) * call->rectSize.X,
						(1.f + zFudge) * call->rectSize.Y };
			if (entity->type == EntityType_Player) {
				int breakHere = 5;
			}
			if (call->bitmap) {
				V2 offset = {
					center.X - call->offset.X * pixelsPerMeter,
					center.Y + call->offset.Y * pixelsPerMeter
				};
				RenderBitmap(bitmap, *call->bitmap, offset);
			}
			else {
				V2 min = center - size / 2.f * pixelsPerMeter;
				V2 max = min + size * pixelsPerMeter;
				RenderRectangle(bitmap, min, max, call->R, call->G, call->B);
			}
		}
	}

	EndSimulation(simArena, *simRegion, world);
}