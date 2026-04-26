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
    textColor: 'text-amber-900'
  },
  {
    id: '2000s',
    title: '2000s: A New Chapter',
    subtitle: 'Start of a new life',
    color: 'bg-blue-100', // Calmer, transition color
    textColor: 'text-blue-900'
  },
  {
    id: '2010s',
    title: '2010s: Big Things',
    subtitle: 'When things were a little simpler',
    color: 'bg-purple-100', // Soft transition color
    textColor: 'text-purple-900'
  },
  {
    id: 'present',
    title: 'Today',
    subtitle: 'Thank you for everything.',
    color: 'bg-rose-100', // Warm, loving color
    textColor: 'text-rose-900'
  }
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
            viewport={{ once: false, amount: 0.5 }}
            transition={{ duration: 0.8 }}
            className="text-center max-w-4xl"
          >
            <h1 className="text-5xl md:text-7xl font-bold mb-6 tracking-wide">
              {decade.title}
            </h1>
            <p className="text-2xl font-light opacity-80">
              {decade.subtitle}
            </p>
          </motion.div>
        </section>
      ))}
    </div>
  );
}