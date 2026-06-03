import { motion } from 'framer-motion';

// 1. The Data Structure: Easy to update and add to!
const timelineData = [
  {
    id: 'intro',
    title: 'Happy Mother\'s Day',
    subtitle: 'Mini picture book',
    color: 'bg-slate-900', // Dark starting screen
    textColor: 'text-emerald-400'
  },
  {
    id: 'cuba',
    title: '1960s - 1990s: Cuba',
    subtitle: 'Your home country',
    color: 'bg-orange-100', // Vintage/Sepia vibe
    textColor: 'text-amber-900',
    image: [
      {id: 'cuba-image-01', src: '../images/Screenshot-2026-04-30-122316.png'},
      {id: 'cuba-image-02', src: '../images/Screenshot-2026-04-30-122345.png'},
      {id: 'cuba-image-03', src: '../images/Screenshot-2026-04-30-122507.png'},
      {id: 'cuba-image-04', src: '../images/Screenshot-2026-04-30-122613.png'},
      {id: 'cuba-image-05', src: '../images/Screenshot-2026-04-30-122832.png'},
    ]
  },
  {
    id: '2000s',
    title: '2000s: A New Chapter',
    subtitle: 'Start of a new life',
    color: 'bg-blue-100', // Calmer, transition color
    textColor: 'text-blue-900',
    image: [
      {id: '2000s-image-01', src: '../images/Screenshot-2026-04-30-122937.png'},
      {id: '2000s-image-02', src: '../images/Screenshot-2026-04-30-123020.png'},
      {id: '2000s-image-03', src: '../images/Screenshot-2026-04-30-123542.png'},
      {id: '2000s-image-04', src: '../images/Screenshot-2026-04-30-123601.png'},
      {id: '2000s-image-05', src: '../images/Screenshot-2026-04-30-123624.png'},
    ]
  },
  {
    id: '2010s',
    title: '2010s: Big Things',
    subtitle: 'When things were a little simpler',
    color: 'bg-purple-100', // Soft transition color
    textColor: 'text-purple-900',
    image: [
      {id: '2010s-image-01', src: '../images/Screenshot-2026-04-30-123832.png'},
      {id: '2010s-image-02', src: '../images/Screenshot-2026-04-30-123845.png'},
      {id: '2010s-image-03', src: '../images/Screenshot-2026-04-30-123927.png'},
      {id: '2010s-image-04', src: '../images/Screenshot-2026-04-30-124049.png'},
      {id: '2010s-image-05', src: '../images/Screenshot-2026-04-30-124205.png'},
    ]
  },
  {
    id: 'present',
    title: 'Today',
    subtitle: 'Thank you for everything.',
    color: 'bg-rose-100', // Warm, loving color
    textColor: 'text-rose-900',
    image: [
      {id: 'present-image-01', src: '../images/Screenshot-2026-04-30-124311.png'},
      {id: 'present-image-02', src: '../images/Screenshot-2026-04-30-124331.png'},
      {id: 'present-image-03', src: '../images/Screenshot-2026-04-30-124404.png'},
      {id: 'present-image-04', src: '../images/Screenshot-2026-04-30-124415.png'},
      {id: 'present-image-05', src: '../images/Screenshot-2026-04-30-124601.png'},
    ]
  },
];

export default function App() {
  return (
    <div className="w-full">
      {timelineData.map((decade) => (
        <section 
          key={decade.id} 
          className={`min-h-screen w-full flex flex-col items-center justify-center p-8 transition-colors duration-1000 ${decade.color} ${decade.textColor}`}
        >
          <motion.div
            initial={{ opacity: 0, y: 50 }}
            whileInView={{ opacity: 1, y: 0 }}
            viewport={{ once: false, amount: 0.2 }}
            transition={{ duration: 0.8 }}
            className="text-center max-w-4xl"
          >
            <h1 className="text-5xl md:text-7xl font-bold mb-6 tracking-wide">
              {decade.title}
            </h1>
            <p className="text-2xl font-light opacity-80">
              {decade.subtitle}
            </p>

            <div className={'flex flex-col items-center justify-center gap-4'}>
              {decade.image?.map((picture) => (
                <div
                  key = {picture.id}
                  className="w-120 h-120 overflow-hidden rounded-xl shadow-lg" 
                >
                  <img src={picture.src} 
                  alt="Yeah I guess it's not loading" 
                  className="w-full h-full object-cover"/>
                </div>
              ))}
            </div>
          </motion.div>
        </section>
      ))}
      <section 
        className={`min-h-screen w-full flex flex-col items-center justify-center p-8 transition-colors duration-1000 bg-slate-900 text-[#e88310]`}
      >
        <motion.div
          initial={{ opacity: 0, y: 50 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: false, amount: 0.2 }}
          transition={{ duration: 0.8 }}
          className="text-center max-w-4xl"
        >
          <h1 className="text-5xl md:text-7xl font-bold mb-6 tracking-wide">
            final message
          </h1>
          <p className="text-2xl font-bold opacity-80">
            I didn't know what to get you for mothers day so I just decided to do what I do best which is things
            I'm good at programming and decided to do that for you instead. I don't talk much cuz I don't have 
            anything to say but I do care. Happy mothers day.
          </p>
        </motion.div>
      </section>
    </div>
  );
}